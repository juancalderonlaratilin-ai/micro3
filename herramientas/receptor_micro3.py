#!/usr/bin/env python3
"""Receptor de las líneas ASCII de I y Q del PIC16F18426 (Micro 3).

El PIC manda, sin que se le pida nada, 500 líneas por segundo así:
    I:2048,Q:2051\r\n
(I y Q son códigos del ADC de 0 a 4095; 1 código = 3,7 V / 4096 = 0,903 mV).

Este programa:
  1. abre el puerto a 115200 8N1;
  2. junta los bytes y, en cada línea completa, saca I y Q;
  3. cuenta las líneas que llegan cortadas o con otra forma ("líneas malas");
  4. grafica I y Q en voltios en vivo;
  5. al terminar cierra el puerto y guarda un CSV.

Para las mediciones (SINAD, ENOB, ruido, histograma) usa interfaz_micro3.py.

Requiere:  pip install pyserial matplotlib

Uso (Windows):
  python receptor_micro3.py COM5                       se detiene al cerrar la gráfica
  python receptor_micro3.py COM5 --muestras 10000      se detiene solo a las 10 000
  python receptor_micro3.py COM5 --csv captura.csv     nombre del archivo
  python receptor_micro3.py COM5 --sin-grafica         solo consola (Ctrl+C para parar)
  python receptor_micro3.py --simular                  prueba sin PIC (datos inventados)
"""
import argparse
import collections
import csv
import math
import re
import threading
import time

LINEA = re.compile(rb"I:(\d{1,5}),Q:(\d{1,5})\r?\n")
FS = 500.0                                   # líneas por segundo (ADC a 1 kHz, 1 de cada 2)
VOLTIOS_POR_CUENTA = 3.7 / 4096              # 1 LSB = 0,903 mV


class Decodificador:
    """Recibe bytes en cualquier cantidad y saca I y Q de cada línea completa."""

    def __init__(self):
        self.texto = bytearray()
        self.i, self.q = [], []
        self.lineas_malas = 0

    def alimentar(self, datos):
        """Agrega bytes y devuelve la lista de (i, q) nuevos."""
        self.texto += datos
        nuevos = []
        fin = self.texto.rfind(b"\n")
        if fin < 0:
            if len(self.texto) > 4096:       # mucho sin salto de línea: es basura
                self.lineas_malas += 1
                self.texto.clear()
            return nuevos
        bloque = bytes(self.texto[:fin + 1])
        del self.texto[:fin + 1]
        for m in LINEA.finditer(bloque):
            i, q = int(m.group(1)), int(m.group(2))
            self.i.append(i)
            self.q.append(q)
            nuevos.append((i, q))
        # "Micro3 listo" (el saludo del PIC al arrancar) no cuenta como línea mala
        self.lineas_malas += max(0, bloque.count(b"\n") - len(nuevos) - bloque.count(b"Micro3"))
        return nuevos


class PuertoSimulado:
    """Imita al PIC: I y Q senoidales de 20 Hz, 500 líneas por segundo."""

    def __init__(self):
        self.salida = bytearray(b"Micro3 listo\r\n")
        self.n = 0
        self.t0 = time.perf_counter()
        self.lock = threading.Lock()

    def _generar(self):
        objetivo = int((time.perf_counter() - self.t0) * FS)
        while self.n < objetivo:
            t = self.n / FS
            i = round(2048 + 1880 * math.sin(2 * math.pi * 20 * t))
            q = round(2048 + 1880 * math.cos(2 * math.pi * 20 * t))
            self.salida += f"I:{i},Q:{q}\r\n".encode("ascii")
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


def hilo_lector(puerto, deco, parar, nuevos_cola, max_muestras):
    """Lee el puerto sin parar, decodifica y deja los datos nuevos en una cola."""
    while not parar.is_set():
        n = puerto.in_waiting
        datos = puerto.read(n if n else 1)
        if not datos:
            continue
        nuevos_cola.extend(deco.alimentar(datos))
        if max_muestras and len(deco.i) >= max_muestras:
            parar.set()


def guardar_csv(ruta, deco):
    with open(ruta, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["n", "t_s", "i", "q", "i_V", "q_V"])
        for k, (i, q) in enumerate(zip(deco.i, deco.q)):
            w.writerow([k, f"{k / FS:.4f}", i, q,
                        f"{i * VOLTIOS_POR_CUENTA:.5f}", f"{q * VOLTIOS_POR_CUENTA:.5f}"])


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
    ax.set_ylim(-0.05, 3.75)
    ax.set_xlabel(f"muestras (últimas {ventana}, 2 ms cada una)")
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
        titulo.set_text(f"muestras: {len(deco.i)}   líneas malas: {deco.lineas_malas}")
        if parar.is_set():
            plt.close(fig)
        return linea_i, linea_q, titulo

    fig.canvas.mpl_connect("close_event", lambda _e: parar.set())
    anim = FuncAnimation(fig, actualizar, interval=50, blit=False, cache_frame_data=False)
    return fig, anim


def main():
    ap = argparse.ArgumentParser(description="Receptor de las líneas I:...,Q:... de Micro 3")
    ap.add_argument("puerto", nargs="?", help="puerto del CP2102, p. ej. COM5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--muestras", type=int, default=0, help="parar al llegar a N (0 = parar a mano)")
    ap.add_argument("--csv", default="captura_micro3.csv", help="archivo de salida")
    ap.add_argument("--ventana", type=int, default=1000, help="muestras visibles en la gráfica")
    ap.add_argument("--sin-grafica", action="store_true", help="no abrir la gráfica")
    ap.add_argument("--simular", action="store_true", help="probar sin PIC")
    args = ap.parse_args()

    if args.simular:
        puerto = PuertoSimulado()
        print("Modo simulación: senoidales de 20 Hz inventadas, 500 líneas/s.")
    else:
        if not args.puerto:
            ap.error("falta el puerto (por ejemplo COM5), o usa --simular")
        import serial
        puerto = serial.Serial(args.puerto, args.baud, bytesize=8, parity="N", stopbits=1, timeout=0.05)
        print(f"Puerto {args.puerto} abierto a {args.baud} 8N1.")

    deco = Decodificador()
    parar = threading.Event()
    cola = collections.deque()

    time.sleep(0.1)
    puerto.reset_input_buffer()               # tira lo que llegó antes (una línea a medias)
    t_inicio = time.perf_counter()
    hilo = threading.Thread(target=hilo_lector, daemon=True,
                            args=(puerto, deco, parar, cola, args.muestras))
    hilo.start()

    time.sleep(1.0)
    if not deco.i:
        print("AVISO: no llega ninguna línea I:...,Q:... Revisa el COM, JP_TX y que el PIC esté programado.")
    else:
        print("Llegan líneas: el PIC está enviando.")

    try:
        if args.sin_grafica:
            print("Recibiendo... (Ctrl+C para parar)")
            while not parar.is_set():
                time.sleep(1.0)
                print(f"  muestras {len(deco.i):7d}   líneas malas {deco.lineas_malas}")
        else:
            import matplotlib.pyplot as plt
            fig, anim = crear_grafica(args.ventana, cola, deco, parar)
            print("Recibiendo... (cierra la ventana de la gráfica para parar)")
            plt.show()
    except KeyboardInterrupt:
        pass
    finally:
        parar.set()
        hilo.join(timeout=1.0)
        duracion = time.perf_counter() - t_inicio
        puerto.close()

    guardar_csv(args.csv, deco)
    n = len(deco.i)
    print("\n--- Resumen ---")
    print(f"Muestras recibidas : {n}")
    print(f"Duración           : {duracion:.1f} s  ({n / duracion if duracion else 0:.0f} muestras/s)")
    print(f"Líneas malas       : {deco.lineas_malas}")
    if n:
        print(f"I: promedio {sum(deco.i) / n:.1f}  mín {min(deco.i)}  máx {max(deco.i)}")
        print(f"Q: promedio {sum(deco.q) / n:.1f}  mín {min(deco.q)}  máx {max(deco.q)}")
    print(f"CSV guardado en    : {args.csv}")


if __name__ == "__main__":
    main()
