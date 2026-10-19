#!/usr/bin/env python3
"""Prueba de eco de la UART del PIC16F18426 (Fase 1).

Requiere:  pip install pyserial
Uso:       python prueba_uart.py COM5            (en Windows; mira el número en el Administrador de dispositivos)
           python prueba_uart.py COM5 --baud 115200

El PIC debe estar programado con el programa de eco de la tarea T-03:
cada byte que recibe, lo devuelve igual.

Pruebas (números de docs/protocolos_prueba.md):
  1.5  eco de 1 byte (0x41), 20 veces
  1.6  los 256 valores, uno por uno
  1.7  ráfaga de 1000 bytes seguidos
"""
import argparse
import sys
import time

import serial  # pyserial


def abrir(puerto, baud):
    """Abre el puerto serie en 8N1 con un tiempo de espera corto para no colgarse."""
    # serial_for_url permite usar "loop://" para probar este script sin hardware
    return serial.serial_for_url(
        puerto, baudrate=baud, bytesize=8, parity="N", stopbits=1, timeout=0.5
    )


def vaciar_entrada(ser):
    """Descarta lo que haya llegado antes (por ejemplo el saludo del arranque)."""
    time.sleep(0.2)
    ser.reset_input_buffer()


def prueba_1_5(ser):
    """Eco de 1 byte: manda 0x41 y espera que regrese 0x41. 20 de 20."""
    buenos = 0
    for _ in range(20):
        ser.write(b"\x41")
        r = ser.read(1)
        if r == b"\x41":
            buenos += 1
    print(f"1.5  eco de 1 byte:        {buenos}/20   (criterio: 20/20)")
    return buenos == 20


def prueba_1_6(ser):
    """Los 256 valores (0x00 a 0xFF), uno por uno. 256 de 256 idénticos."""
    buenos = 0
    malos = []
    for v in range(256):
        ser.write(bytes([v]))
        r = ser.read(1)
        if r == bytes([v]):
            buenos += 1
        else:
            malos.append((v, r))
    print(f"1.6  256 valores:          {buenos}/256 (criterio: 256/256)")
    for v, r in malos[:5]:
        print(f"       mandé 0x{v:02X}, recibí {r.hex() or 'nada'}")
    return buenos == 256


def prueba_1_7(ser, n=1000):
    """Ráfaga: manda n bytes seguidos y compara lo que regresa. Debe ser idéntico."""
    datos = bytes((i * 7 + 3) & 0xFF for i in range(n))  # patrón fijo, no aleatorio
    vaciar_entrada(ser)
    t0 = time.time()
    ser.write(datos)
    ser.flush()
    recibido = ser.read(n)  # espera hasta n bytes o el timeout
    dt = time.time() - t0
    iguales = sum(1 for a, b in zip(datos, recibido) if a == b)
    ok = recibido == datos
    print(
        f"1.7  ráfaga de {n} bytes:  recibí {len(recibido)}/{n}, iguales {iguales}/{n}, "
        f"{dt * 1000:.0f} ms (mínimo teórico {n * 86.8e-3:.0f} ms a 115200)"
    )
    if not ok:
        for i, (a, b) in enumerate(zip(datos, recibido)):
            if a != b:
                print(f"       primer fallo en el byte {i}: mandé 0x{a:02X}, recibí 0x{b:02X}")
                break
        else:
            print(f"       faltan {n - len(recibido)} bytes (se perdieron al final)")
    return ok


def main():
    ap = argparse.ArgumentParser(description="Prueba de eco de la UART del PIC")
    ap.add_argument("puerto", help="COMx en Windows (o loop:// para probar el script sin hardware)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    try:
        ser = abrir(args.puerto, args.baud)
    except serial.SerialException as e:
        print(f"No pude abrir {args.puerto}: {e}")
        print("¿Está abierto en otro programa (terminal, MPLAB)? ¿Es el COM correcto?")
        return 2

    with ser:
        vaciar_entrada(ser)
        resultados = [prueba_1_5(ser), prueba_1_6(ser), prueba_1_7(ser)]

    if all(resultados):
        print("\nTODAS PASARON")
        return 0
    print("\nHAY FALLAS (ver arriba y la tabla de síntomas en docs/guia_prueba_uart_snap.md)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
