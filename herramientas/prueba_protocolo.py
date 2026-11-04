#!/usr/bin/env python3
"""Prueba del protocolo de tramas con CRC-8 del PIC16F18426 (tarea T-04, Módulo 2).

Requiere:  pip install pyserial
Uso:       python prueba_protocolo.py COM5
           python prueba_protocolo.py COM5 --solo 2.3 2.4        (solo esas pruebas)
           python prueba_protocolo.py COM5 --incluir 2.11 2.12   (también las interactivas)

El PIC debe estar programado con T-04 y MODO_ECO = 0 en main.c (protocolo, no eco).

Trama de 9 bytes:  A5 5A | TYPE | SEQ | PAYLOAD(4) | CRC8   (CRC sobre los bytes 2 a 7)
  PING 0x01 (PC -> PIC)   PONG 0x81 (PIC -> PC)   ERROR 0x7F (PIC -> PC, P0 = código)
  Códigos de ERROR: 1 CRC, 2 TYPE desconocido, 3 overflow de RX, 4 framing/overrun.

El CRC se calcula aquí por su cuenta (crc8_vectores.py), independiente del PIC.

Pruebas (números de docs/protocolos_prueba.md y docs/especificacion_protocolo.md §8):
  2.13 vector "123456789" = 0xF4 (valida este script)      2.1  CRC del PIC contra el del PC
  2.2  PING válido (20)     2.3  CRC alterado (10)         2.4  TYPE desconocido (10)
  2.5  basura antes de un PING (20)    2.6  trama partida (10)
  2.7  1000 PING seguidos, más ráfagas de 3 tramas
  2.8  latencia: aquí solo se muestra el tiempo de ida y vuelta del PC (orientativo);
       la medida de verdad es con el osciloscopio
  2.9  falso encabezado A5 A5 5A (10)     2.10  A5 5A dentro del payload (10)
  2.11 overflow del RX (interactiva, necesita el depurador)    2.12 framing (interactiva)
"""
import argparse
import random
import sys
import time

import serial  # pyserial

from crc8_vectores import crc8
from prueba_uart import abrir, vaciar_entrada

SYNC = b"\xA5\x5A"
PING, PONG, ERROR = 0x01, 0x81, 0x7F


def trama(tipo, seq, payload=b"\0\0\0\0", crc=None):
    """Arma los 9 bytes. Si se da `crc`, se usa ese valor (para dañar la trama a propósito)."""
    cuerpo = bytes([tipo, seq]) + bytes(payload)
    return SYNC + cuerpo + bytes([crc8(cuerpo) if crc is None else crc])


class Lector:
    """Junta los bytes que llegan y entrega tramas completas, ignorando lo que no sea A5 5A."""

    def __init__(self, ser):
        self.ser = ser
        self.buf = bytearray()

    def siguiente(self, espera=0.5):
        """Devuelve (bytes_de_la_trama, crc_ok) o None si no llega ninguna en `espera` s."""
        fin = time.time() + espera
        while True:
            i = self.buf.find(SYNC)
            if i >= 0 and len(self.buf) - i >= 9:
                t = bytes(self.buf[i:i + 9])
                del self.buf[:i + 9]
                return t, crc8(t[2:8]) == t[8]
            if time.time() > fin:
                return None
            self.buf += self.ser.read(max(1, self.ser.in_waiting))

    def esperar(self, tipo, seq=None, espera=0.5):
        """Lee tramas hasta encontrar `tipo` (con ese seq si se da). Devuelve (trama, errores_vistos).
        Los ERROR que lleguen antes se guardan en la lista; otras tramas se ignoran."""
        errores = []
        fin = time.time() + espera
        while time.time() < fin:
            r = self.siguiente(max(0.01, fin - time.time()))
            if r is None:
                break
            t, ok = r
            if not ok:
                continue
            if t[2] == tipo and (seq is None or t[3] == seq):
                return t, errores
            if t[2] == ERROR:
                errores.append(t[4])
        return None, errores

    def limpiar(self):
        vaciar_entrada(self.ser)
        self.buf.clear()


def ping_ok(lec, ser, seq, payload):
    """Manda un PING y comprueba que vuelva el PONG con el mismo seq, payload y CRC bueno."""
    ser.write(trama(PING, seq, payload))
    t, err = lec.esperar(PONG, seq)
    return t is not None and t[4:8] == bytes(payload), err


def informe(nombre, buenos, total, extra=""):
    ok = buenos == total
    print(f"{nombre:<34} {buenos}/{total}  {'OK' if ok else 'FALLA'}  {extra}")
    return ok


def p_2_13(lec, ser):
    return informe("2.13 vector '123456789'", int(crc8(b"123456789") == 0xF4), 1,
                   f"(CRC = 0x{crc8(b'123456789'):02X}, esperado 0xF4)")


def p_2_1(lec, ser):
    buenos = 0
    for k, pl in enumerate([b"\x00\x00\x00\x00", b"\x11\x22\x33\x44", b"\xFF\x01\x80\x7F"]):
        ser.write(trama(PING, k, pl))
        t, _ = lec.esperar(PONG, k)
        esperado = crc8(bytes([PONG, k]) + pl)
        if t is not None and t[8] == esperado:
            buenos += 1
        else:
            print(f"       PING {k}: PIC {'nada' if t is None else hex(t[8])}, PC 0x{esperado:02X}")
    return informe("2.1  CRC del PIC = CRC del PC", buenos, 3)


def p_2_2(lec, ser):
    buenos = sum(ping_ok(lec, ser, i, bytes([i, i ^ 0xFF, 0x5A, i * 3 & 0xFF]))[0] for i in range(20))
    return informe("2.2  PING válido", buenos, 20)


def p_2_3(lec, ser):
    buenos = 0
    for i in range(10):
        bueno = trama(PING, i, b"\x01\x02\x03\x04")
        ser.write(bueno[:8] + bytes([bueno[8] ^ (1 << (i % 8))]))  # un bit del CRC cambiado
        t, err = lec.esperar(PONG, i, espera=0.3)
        vivo, _ = ping_ok(lec, ser, 100 + i, b"\xAA\xBB\xCC\xDD")
        if t is None and 1 in err and vivo:
            buenos += 1
    return informe("2.3  CRC alterado -> ERROR 1", buenos, 10, "(y el PIC sigue respondiendo)")


def p_2_4(lec, ser):
    buenos = 0
    for i in range(10):
        ser.write(trama(0x55, i))
        t, err = lec.esperar(ERROR, i, espera=0.3)
        if t is not None and t[4] == 2:
            buenos += 1
    return informe("2.4  TYPE 0x55 -> ERROR 2", buenos, 10)


def p_2_5(lec, ser):
    rnd = random.Random(1234)
    buenos = 0
    for i in range(20):
        basura = bytes(rnd.randrange(256) for _ in range(rnd.randrange(1, 12)))
        ser.write(basura + trama(PING, i, b"\x10\x20\x30\x40"))
        t, _ = lec.esperar(PONG, i)
        # Si la basura trae un A5 5A, puede salir un ERROR 1 antes del PONG: es lo esperado.
        if t is not None:
            buenos += 1
    return informe("2.5  basura + PING", buenos, 20)


def p_2_6(lec, ser):
    buenos = 0
    for i in range(10):
        t0 = trama(PING, i, b"\x0A\x0B\x0C\x0D")
        ser.write(t0[:4])
        ser.flush()
        time.sleep(0.05)
        ser.write(t0[4:])
        t, _ = lec.esperar(PONG, i)
        buenos += t is not None
    return informe("2.6  trama partida (50 ms)", buenos, 10)


def p_2_7(lec, ser):
    n = 1000
    buenos = 0
    t0 = time.time()
    for i in range(n):
        buenos += ping_ok(lec, ser, i & 0xFF, bytes([i & 0xFF, i >> 8, 0, 1]))[0]
    dt = time.time() - t0
    ok1 = informe("2.7  1000 PING seguidos", buenos, n, f"({dt:.1f} s)")
    rafagas = 0
    for k in range(100):  # 3 tramas = 27 bytes, caben en el buffer de RX (31 útiles)
        seqs = [(k * 3 + j) & 0xFF for j in range(3)]
        ser.write(b"".join(trama(PING, s, bytes([s, 0, 0, 0])) for s in seqs))
        vistos = []
        for _ in range(3):
            t, _ = lec.esperar(PONG)
            if t is not None:
                vistos.append(t[3])
        rafagas += vistos == seqs
    ok2 = informe("2.7b ráfagas de 3 PING", rafagas, 100)
    return ok1 and ok2


def p_2_8(lec, ser):
    tiempos = []
    for i in range(50):
        t0 = time.perf_counter()
        ping_ok(lec, ser, i, b"\0\0\0\0")
        tiempos.append((time.perf_counter() - t0) * 1000)
    print(f"2.8  ida y vuelta en el PC: mediana {sorted(tiempos)[25]:.2f} ms, máx {max(tiempos):.2f} ms"
          " (incluye el USB; la medida válida es con el osciloscopio, criterio <= 2 ms)")
    return True


def p_2_9(lec, ser):
    buenos = 0
    for i in range(10):
        ser.write(b"\xA5\xA5\x5A" + trama(PING, i, b"\x01\x02\x03\x04"))
        t, err = lec.esperar(PONG, i)
        buenos += t is not None  # un ERROR 1 antes del PONG es normal (PE5)
    return informe("2.9  falso encabezado A5 A5 5A", buenos, 10, "(un ERROR 1 antes del PONG es normal)")


def p_2_10(lec, ser):
    buenos = 0
    for i in range(10):
        ok, _ = ping_ok(lec, ser, i, b"\xA5\x5A\x00\x00")
        ok2, _ = ping_ok(lec, ser, i + 50, b"\x00\x00\xA5\x5A")
        buenos += ok and ok2
    return informe("2.10 A5 5A dentro del payload", buenos, 10)


def p_2_11(lec, ser):
    print("2.11 Overflow del RX (interactiva).")
    print("     1) En MPLAB X pon un punto de parada en la primera línea de protocol_task() y")
    print("        arranca en modo depuración.  2) Pulsa Enter aquí: se envían 40 bytes (0x00).")
    input("     Enter para enviar... ")
    ser.write(b"\x00" * 40)
    input("     Ahora quita el punto de parada y deja correr el programa; Enter cuando corra... ")
    lec.buf.clear()
    t, err = lec.esperar(ERROR, espera=1.0)
    visto = t is not None and t[4] == 3 or 3 in err
    vivo, _ = ping_ok(lec, ser, 1, b"\x01\x02\x03\x04")
    return informe("2.11 overflow -> ERROR 3 y sigue", int(visto and vivo), 1,
                   f"(ERROR 3 visto: {visto}, responde PING: {vivo})")


def p_2_12(lec, ser):
    print("2.12 Framing (interactiva, opcional): se cambia el baud del PC a 9600 un instante.")
    lec.limpiar()
    base = ser.baudrate
    ser.baudrate = 9600
    ser.write(b"\x00\x00\x00")
    ser.flush()
    time.sleep(0.05)
    ser.baudrate = base
    t, err = lec.esperar(ERROR, espera=1.0)
    visto = (t is not None and t[4] == 4) or 4 in err
    lec.limpiar()
    vivo = any(ping_ok(lec, ser, i, b"\x01\x02\x03\x04")[0] for i in range(3))
    return informe("2.12 framing -> ERROR 4 y se recupera", int(visto and vivo), 1,
                   f"(ERROR 4 visto: {visto}, responde PING: {vivo})")


PRUEBAS = {"2.13": p_2_13, "2.1": p_2_1, "2.2": p_2_2, "2.3": p_2_3, "2.4": p_2_4, "2.5": p_2_5,
           "2.6": p_2_6, "2.7": p_2_7, "2.8": p_2_8, "2.9": p_2_9, "2.10": p_2_10,
           "2.11": p_2_11, "2.12": p_2_12}
INTERACTIVAS = {"2.11", "2.12"}


def main():
    ap = argparse.ArgumentParser(description="Prueba del protocolo de tramas del PIC (T-04)")
    ap.add_argument("puerto", help="COMx en Windows")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--solo", nargs="+", metavar="N", help="correr solo estas pruebas, p. ej. 2.3 2.4")
    ap.add_argument("--incluir", nargs="+", metavar="N", default=[],
                    help="agregar las interactivas (2.11, 2.12) a la corrida normal")
    args = ap.parse_args()

    lista = args.solo or [n for n in PRUEBAS if n not in INTERACTIVAS] + args.incluir
    malas = [n for n in lista if n not in PRUEBAS]
    if malas:
        print(f"Pruebas desconocidas: {malas}. Válidas: {list(PRUEBAS)}")
        return 2

    try:
        ser = abrir(args.puerto, args.baud)
    except serial.SerialException as e:
        print(f"No pude abrir {args.puerto}: {e}")
        return 2

    resultados = {}
    with ser:
        lec = Lector(ser)
        lec.limpiar()
        for n in lista:
            resultados[n] = PRUEBAS[n](lec, ser)
            lec.limpiar()

    fallas = [n for n, ok in resultados.items() if not ok]
    if not fallas:
        print("\nTODAS PASARON (2.8 y la medida real de latencia: con el osciloscopio)")
        return 0
    print(f"\nFALLARON: {', '.join(fallas)}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
