#!/usr/bin/env python3
"""Receptor de tramas DATA_IQ del PIC16F18426 (Micro 3).

Hace lo mismo que el VI de LabVIEW:
  1. abre el puerto a 115200 8N1 y manda ADC_STREAM = 1 (encender el envío);
  2. junta los bytes, busca A5 5A, toma 9 bytes y comprueba el CRC-8;
  3. de cada trama DATA_IQ (TYPE 0xA1) saca SEQ, I y Q;
  4. cuenta tramas con CRC malo y tramas perdidas (saltos de SEQ);
  5. grafica I y Q en voltios en vivo;
  6. al terminar manda ADC_STREAM = 0, cierra el puerto y guarda un CSV.

Trama (9 bytes):  A5 5A | TYPE | SEQ | I_alto I_bajo Q_alto Q_bajo | CRC8
CRC-8: polinomio 0x07, valor inicial 0x00, sobre los bytes 2 a 7.

Requiere:  pip install pyserial matplotlib

Uso (Windows):
  python receptor_micro3.py COM5                       se detiene al cerrar la gráfica
  python receptor_micro3.py COM5 --muestras 10000      se detiene solo a las 10 000
  python receptor_micro3.py COM5 --csv captura.csv     nombre del archivo
  python receptor_micro3.py COM5 --sin-grafica         solo consola (Ctrl+C para parar)
  python receptor_micro3.py COM5 --reenviar COM11      además manda "I:..,Q:.." a otro
                                                       puerto (para un Serial Plotter)
  python receptor_micro3.py --simular                  prueba sin PIC (datos inventados)

El firmware debe estar con MODO_PRUEBA_ADC = 0 y MODO_ECO = 0.
"""
import argparse
import collections
import csv
import math
import sys
import threading
import time

SYNC = b"\xA5\x5A"
LARGO = 9
TIPO_DATA_IQ = 0xA1
TIPO_ADC_ACK = 0xA0
TIPO_ERROR = 0x7F
TIPO_PONG = 0x81
CMD_ENCENDER = bytes.fromhex("A55A2000010000002D")   # ADC_STREAM, SEQ 0, P0 = 1
CMD_APAGAR = bytes.fromhex("A55A20010000000059")     # ADC_STREAM, SEQ 1, P0 = 0
VOLTIOS_POR_CUENTA = 3.7 / 4096                      # 1 LSB = 0,903 mV


def crc8(datos):
    """CRC-8, polinomio 0x07, inicial 0x00 (igual que crc8.c del PIC)."""
    crc = 0
    for b in datos:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


class Decodificador:
    """Recibe bytes en cualquier cantidad y saca las tramas completas."""

    def __init__(self):
        self.buffer = bytearray()
        self.seq, self.i, self.q = [], [], []
        self.crc_malos = 0
        self.perdidas = 0
        self.seq_anterior = None
        self.envio_encendido = None
        self.errores_pic = collections.Counter()

    def alimentar(self, datos):
        """Agrega bytes al buffer y devuelve la lista de (seq, i, q) nuevos."""
        self.buffer += datos
        nuevos = []
        while True:
            k = self.buffer.find(SYNC)
            if k < 0:
                # no hay encabezado: se guarda solo el último byte si es A5 (mitad de un encabezado)
                del self.buffer[:-1]
                if self.buffer[:1] != b"\xA5":
                    self.buffer.clear()
                break
            if k > 0:
                del self.buffer[:k]           # basura antes del encabezado (p. ej. "Micro3 listo")
            if len(self.buffer) < LARGO:
                break                         # trama incompleta: se termina con la próxima lectura
            t = bytes(self.buffer[:LARGO])
            if crc8(t[2:8]) != t[8]:
                self.crc_malos += 1
                del self.buffer[:1]           # resincronizar: quitar solo el primer byte
                continue
            del self.buffer[:LARGO]
            tipo, seq = t[2], t[3]
            if tipo == TIPO_DATA_IQ:
                i = (t[4] << 8) | t[5]
                q = (t[6] << 8) | t[7]
                if self.seq_anterior is not None:
                    self.perdidas += (seq - self.seq_anterior - 1) % 256
                self.seq_anterior = seq
                self.seq.append(seq)
                self.i.append(i)
                self.q.append(q)
                nuevos.append((seq, i, q))
            elif tipo == TIPO_ADC_ACK:
                self.envio_encendido = t[4] != 0
            elif tipo == TIPO_ERROR:
                self.errores_pic[t[4]] += 1
        return nuevos


class PuertoSimulado:
    """Imita al PIC: responde ADC_STREAM y manda DATA_IQ a 1 kHz con senoidales.
    Daña 1 trama de cada 500 y se salta 1 de cada 1000, para probar el CRC y el SEQ."""

    def __init__(self):
        self.salida = bytearray(b"Micro3 listo\r\n")
        self.encendido = False
        self.seq = 0
        self.n = 0
        self.t0 = time.perf_counter()
        self.lock = threading.Lock()

    def _trama(self, tipo, seq, payload):
        cuerpo = bytes([tipo, seq]) + bytes(payload)
        return SYNC + cuerpo + bytes([crc8(cuerpo)])

    def write(self, datos):
        if len(datos) == LARGO and datos[:2] == SYNC and datos[2] == 0x20:
            self.encendido = datos[4] != 0
            with self.lock:
                self.salida += self._trama(TIPO_ADC_ACK, datos[3], [1 if self.encendido else 0, 0, 0, 0])
        return len(datos)

    def _generar(self):
        if not self.encendido:
            return
        objetivo = int((time.perf_counter() - self.t0) * 1000)
        while self.n < objetivo:
            t = self.n / 1000.0
            i = int(2048 + 1500 * math.sin(2 * math.pi * 5 * t))
            q = int(2048 + 1500 * math.cos(2 * math.pi * 5 * t))
            tr = bytearray(self._trama(TIPO_DATA_IQ, self.seq, [i >> 8, i & 0xFF, q >> 8, q & 0xFF]))
            if self.n % 500 == 250:
                tr[5] ^= 0x10                 # trama dañada
            if self.n % 1000 != 999:          # trama perdida
                self.salida += tr
            self.seq = (self.seq + 1) & 0xFF
            self.n += 1

    @property
    def in_waiting(self):
        with self.lock:
            self._generar()
            return len(self.salida)

    def read(self, n=1):
        time.sleep(0.005)
        with self.lock:
            self._generar()
            datos = bytes(self.salida[:n])
            del self.salida[:n]
            return datos

    def reset_input_buffer(self):
        with self.lock:
            self.salida.clear()

    def close(self):
        pass


def hilo_lector(puerto, deco, parar, nuevos_cola, reenvio, cada, formato, max_muestras):
    """Lee el puerto sin parar, decodifica y deja los datos nuevos en una cola."""
    contador = 0
    while not parar.is_set():
        n = puerto.in_waiting
        datos = puerto.read(n if n else 1)
        if not datos:
            continue
        for seq, i, q in deco.alimentar(datos):
            nuevos_cola.append((i, q))
            contador += 1
            if reenvio is not None and contador % cada == 0:
                linea = f"I:{i},Q:{q}\n" if formato == "arduino" else f"{i},{q}\n"
                reenvio.write(linea.encode("ascii"))
        if max_muestras and len(deco.i) >= max_muestras:
            parar.set()


def guardar_csv(ruta, deco):
    with open(ruta, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["seq", "i", "q", "i_V", "q_V"])
        for s, i, q in zip(deco.seq, deco.i, deco.q):
            w.writerow([s, i, q, f"{i * VOLTIOS_POR_CUENTA:.5f}", f"{q * VOLTIOS_POR_CUENTA:.5f}"])


def crear_grafica(ventana, cola, deco, parar):
    """Gráfica en vivo de I y Q en voltios (últimas `ventana` muestras)."""
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    ui = collections.deque(maxlen=ventana)
    uq = collections.deque(maxlen=ventana)
    fig, ax = plt.subplots(figsize=(10, 5))
    linea_i, = ax.plot([], [], label="I")
    linea_q, = ax.plot([], [], label="Q")
    ax.set_xlim(0, ventana)
    ax.set_ylim(0, 3.7)
    ax.set_xlabel(f"muestras (últimas {ventana}, 1 ms cada una)")
    ax.set_ylabel("voltios")
    ax.grid(True)
    ax.legend(loc="upper right")
    titulo = ax.set_title("esperando datos...")

    def actualizar(_):
        while cola:
            i, q = cola.popleft()
            ui.append(i * VOLTIOS_POR_CUENTA)
            uq.append(q * VOLTIOS_POR_CUENTA)
        x = range(len(ui))
        linea_i.set_data(x, list(ui))
        linea_q.set_data(x, list(uq))
        titulo.set_text(f"muestras: {len(deco.i)}   CRC malos: {deco.crc_malos}   "
                        f"perdidas (SEQ): {deco.perdidas}")
        if parar.is_set():
            plt.close(fig)
        return linea_i, linea_q, titulo

    fig.canvas.mpl_connect("close_event", lambda _e: parar.set())
    anim = FuncAnimation(fig, actualizar, interval=50, blit=False, cache_frame_data=False)
    return fig, anim, actualizar


def main():
    ap = argparse.ArgumentParser(description="Receptor de tramas DATA_IQ de Micro 3")
    ap.add_argument("puerto", nargs="?", help="puerto del CP2102, p. ej. COM5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--muestras", type=int, default=0, help="parar al llegar a N (0 = parar a mano)")
    ap.add_argument("--csv", default="captura_micro3.csv", help="archivo de salida")
    ap.add_argument("--ventana", type=int, default=2000, help="muestras visibles en la gráfica")
    ap.add_argument("--sin-grafica", action="store_true", help="no abrir la gráfica")
    ap.add_argument("--reenviar", metavar="PUERTO", help="manda I/Q como texto a otro puerto")
    ap.add_argument("--cada", type=int, default=10, help="al reenviar, 1 línea cada N muestras")
    ap.add_argument("--formato", choices=["arduino", "csv"], default="arduino",
                    help='texto reenviado: "I:2048,Q:2051" (arduino) o "2048,2051" (csv)')
    ap.add_argument("--simular", action="store_true", help="probar sin PIC")
    args = ap.parse_args()

    if args.simular:
        puerto = PuertoSimulado()
        print("Modo simulación: datos inventados, con 1 trama dañada cada 500 y 1 perdida cada 1000.")
    else:
        if not args.puerto:
            ap.error("falta el puerto (por ejemplo COM5), o usa --simular")
        import serial
        puerto = serial.Serial(args.puerto, args.baud, bytesize=8, parity="N", stopbits=1, timeout=0.05)
        print(f"Puerto {args.puerto} abierto a {args.baud} 8N1.")

    reenvio = None
    if args.reenviar:
        import serial
        reenvio = serial.Serial(args.reenviar, args.baud, timeout=0)
        print(f"Reenviando texto a {args.reenviar} (1 línea cada {args.cada} muestras).")

    deco = Decodificador()
    parar = threading.Event()
    cola = collections.deque()

    time.sleep(0.1)
    puerto.reset_input_buffer()               # tira lo que llegó antes ("Micro3 listo", basura)
    puerto.write(CMD_ENCENDER)
    t_inicio = time.perf_counter()
    hilo = threading.Thread(target=hilo_lector, daemon=True,
                            args=(puerto, deco, parar, cola, reenvio, args.cada, args.formato, args.muestras))
    hilo.start()

    time.sleep(1.0)
    if deco.envio_encendido is None:
        print("AVISO: no llegó el ADC_ACK. Revisa MODO_PRUEBA_ADC = 0, JP_TX/JP_RX cerrados y el COM.")
    else:
        print("ADC_ACK recibido: el PIC está enviando.")

    try:
        if args.sin_grafica:
            print("Recibiendo... (Ctrl+C para parar)")
            while not parar.is_set():
                time.sleep(1.0)
                print(f"  muestras {len(deco.i):7d}   CRC malos {deco.crc_malos}   perdidas {deco.perdidas}")
        else:
            import matplotlib.pyplot as plt
            fig, anim, _ = crear_grafica(args.ventana, cola, deco, parar)
            print("Recibiendo... (cierra la ventana de la gráfica para parar)")
            plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        parar.set()
        hilo.join(timeout=1.0)
        duracion = time.perf_counter() - t_inicio
        puerto.write(CMD_APAGAR)
        time.sleep(0.05)
        puerto.close()
        if reenvio is not None:
            reenvio.close()

    guardar_csv(args.csv, deco)
    n = len(deco.i)
    print("\n--- Resumen ---")
    print(f"Muestras recibidas : {n}")
    print(f"Duración           : {duracion:.1f} s  ({n / duracion if duracion else 0:.0f} muestras/s)")
    print(f"CRC malos          : {deco.crc_malos}")
    print(f"Perdidas (SEQ)     : {deco.perdidas}")
    if deco.errores_pic:
        print(f"ERROR del PIC      : {dict(deco.errores_pic)}  (1 CRC, 2 TYPE, 3 RX lleno, 4 framing)")
    if n:
        print(f"I: promedio {sum(deco.i) / n:.1f}  mín {min(deco.i)}  máx {max(deco.i)}")
        print(f"Q: promedio {sum(deco.q) / n:.1f}  mín {min(deco.q)}  máx {max(deco.q)}")
    print(f"CSV guardado en    : {args.csv}")


if __name__ == "__main__":
    main()
