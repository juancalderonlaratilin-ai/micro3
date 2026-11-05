#!/usr/bin/env python3
"""Interfaz de mediciones para Micro 3 (PIC16F18426, envío en texto ASCII).

Recibe por la UART las líneas "2048,2051\r\n" (I,Q) que manda el PIC (adc_ascii.c, 500 por
segundo), muestra I y Q en vivo y calcula las mediciones del ADC:

  * Senoidal: SINAD, ENOB, SNR, THD, SFDR, frecuencia y amplitud (método de
    docs/especificacion_adc.md §6: ventana de Kaiser β = 14, fundamental ±6 bins; además
    se excluye el lóbulo de la continua, bins 0 a 6, que el script original contaba como ruido).
  * Continua: media, ruido rms (σ), pico a pico, códigos distintos, histograma,
    resolución efectiva y libre de ruido (docs/especificacion_adc.md §8).
  * Periodo: si el firmware manda en Q el periodo medido con Timer1 (MODO_PERIODO,
    cuentas de 62,5 ns), histograma del periodo y su desviación estándar.

Requiere:  pip install pyserial numpy matplotlib
Uso:       python interfaz_micro3.py
"""
import collections
import csv
import math
import re
import sys
import threading
import time
try:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
except ImportError:
    print("\nFalta tkinter (la parte de ventanas de Python). Reinstala Python desde python.org\n"
          "y deja marcada la opción «tcl/tk and IDLE».\n")
    input("Presiona Enter para cerrar...")
    sys.exit(1)


def _falta(texto):
    """Muestra qué falta instalar y deja la ventana abierta (si no, se cierra sola)."""
    print("\n" + texto + "\n")
    try:
        r = tk.Tk()
        r.withdraw()
        messagebox.showerror("Falta algo para abrir la interfaz", texto)
        r.destroy()
    except Exception:
        pass
    try:
        input("Presiona Enter para cerrar...")
    except Exception:
        pass
    sys.exit(1)


try:
    import numpy as np
    import matplotlib
    matplotlib.use("TkAgg")
    from matplotlib.figure import Figure
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
    import serial  # noqa: F401  (pyserial)
except ImportError as e:
    _falta(f"Falta un paquete de Python: {e.name}\n\n"
           "Abre cmd y escribe:\n    pip install pyserial numpy matplotlib\n"
           "(o:  py -m pip install pyserial numpy matplotlib)")

FS_ADC = 1000.0              # el ADC muestrea a 1 kHz (Timer0 a 1 ms)
FS = 500.0                   # líneas por segundo que manda el PIC (1 de cada 2 pares)
BAUD = 115200
N_BITS = 12
VREF = 3.7
LSB = VREF / 2 ** N_BITS     # 0,9033 mV
V_FS_RMS = VREF / 2 / math.sqrt(2)   # senoidal a escala completa: 1,308 V rms
T_TIMER1_NS = 62.5           # cuenta de Timer1 a 16 MHz (modo periodo)
SINAD_MIN = 60.0             # requisito


# ============================================================ mediciones (sin interfaz)

def analizar_senoidal(codigos, fs=FS, f_in=None, ancho=6, n_armonicos=5):
    """SINAD y compañía de una captura de una senoidal. Devuelve un dict."""
    x = np.asarray(codigos, dtype=float)
    m = len(x)
    w = np.kaiser(m, 14)
    dc = float((x * w).sum() / w.sum())          # continua ponderada por la ventana
    v = (x - dc) * LSB
    espectro = np.abs(np.fft.rfft(v * w)) ** 2
    s2 = (w ** 2).sum()
    a_vrms2 = lambda p: 2.0 * p / (m * s2)          # potencia de bins -> V rms²

    if f_in:
        k0 = int(round(f_in * m / fs))
    else:
        k0 = int(np.argmax(espectro[ancho + 1:])) + ancho + 1
    lo, hi = max(k0 - ancho, 1), min(k0 + ancho + 1, len(espectro))
    p_sig = espectro[lo:hi].sum()
    # ruido + distorsión: todo menos la fundamental y el lóbulo de la continua (bins 0..ancho).
    # (El script de especificacion_adc.md §6 resta desde el bin 1; con la ventana de Kaiser la
    # continua residual ocupa hasta ~5 bins y bajaba el SINAD, sobre todo a 5 Hz.)
    mascara = np.ones(len(espectro), dtype=bool)
    mascara[:ancho + 1] = False                  # lóbulo de la continua
    mascara[lo:hi] = False                       # fundamental
    p_nd = max(float(espectro[mascara].sum()), 1e-30)
    ks = np.arange(lo, hi)
    k_fino = float((espectro[lo:hi] * ks).sum() / espectro[lo:hi].sum())

    # armónicos (con plegado por encima de fs/2)
    armonicos = []
    p_arm = 0.0
    for h in range(2, n_armonicos + 1):
        kh = (h * k_fino) % m
        if kh > m / 2:
            kh = m - kh
        kh = int(round(kh))
        if kh <= ancho or abs(kh - k0) <= 2 * ancho:
            continue
        a, b = max(kh - ancho, 1), min(kh + ancho + 1, len(espectro))
        p = espectro[a:b].sum()
        armonicos.append((h, kh, p))
        p_arm += p
    p_ruido = max(p_nd - p_arm, 1e-30)

    # mayor espurio (pico de un bin) fuera de la fundamental y de la continua
    resto = espectro.copy()
    resto[:ancho + 1] = 0
    resto[lo:hi] = 0
    k_esp = int(np.argmax(resto))

    sinad = 10 * math.log10(p_sig / p_nd)
    v_sig = math.sqrt(a_vrms2(p_sig))
    r = {
        "muestras": m,
        "frecuencia_Hz": k_fino * fs / m,
        "ciclos": k_fino,
        "dc_codigo": dc,
        "dc_V": dc * LSB,
        "amplitud_pico_V": v_sig * math.sqrt(2),
        "amplitud_rms_V": v_sig,
        "ruido_dist_rms_mV": math.sqrt(a_vrms2(p_nd)) * 1e3,
        "ruido_dist_rms_LSB": math.sqrt(a_vrms2(p_nd)) / LSB,
        "sinad_dB": sinad,
        "enob_bits": (sinad - 1.76) / 6.02,
        "snr_dB": 10 * math.log10(p_sig / p_ruido),
        "thd_dB": 10 * math.log10(p_arm / p_sig) if p_arm > 0 else float("-inf"),
        "sfdr_dB": 10 * math.log10(espectro[lo:hi].max() / max(resto[k_esp], 1e-30)),
        "sinad_fs_dB": sinad + 20 * math.log10(V_FS_RMS / v_sig) if v_sig > 0 else float("nan"),
        "minimo": int(x.min()),
        "maximo": int(x.max()),
        "recorte": bool(x.min() <= 0 or x.max() >= 2 ** N_BITS - 1),
        "pocos_ciclos": k0 < 2 * ancho + 2,
        # para graficar
        "_espectro": espectro,
        "_p_fs": V_FS_RMS ** 2 * m * s2 / 2.0,
        "_k0": k0,
        "_banda": (lo, hi),
        "_armonicos": armonicos,
        "_k_esp": k_esp,
    }
    r["enob_fs_bits"] = (r["sinad_fs_dB"] - 1.76) / 6.02
    return r


def analizar_continua(codigos):
    """Ruido con entrada fija (histograma de códigos)."""
    x = np.asarray(codigos, dtype=float)
    sigma = x.std(ddof=1) if len(x) > 1 else 0.0
    valores = np.unique(x)
    return {
        "muestras": len(x),
        "media_codigo": x.mean(),
        "media_V": x.mean() * LSB,
        "sigma_LSB": sigma,
        "sigma_uV": sigma * LSB * 1e6,
        "pp_codigos": int(x.max() - x.min()),
        "pp_estimado_uV": 6.6 * sigma * LSB * 1e6,
        "codigos_distintos": len(valores),
        "resol_efectiva_bits": math.log2(2 ** N_BITS / sigma) if sigma > 0 else float("inf"),
        "resol_libre_ruido_bits": math.log2(2 ** N_BITS / (6.6 * sigma)) if sigma > 0 else float("inf"),
        "incertidumbre_sigma_pct": 100 / math.sqrt(2 * (len(x) - 1)) if len(x) > 1 else float("nan"),
    }


def analizar_periodo(cuentas):
    """Periodo entre muestras medido por el PIC con Timer1 (cuentas de 62,5 ns)."""
    t = np.asarray(cuentas, dtype=float) * T_TIMER1_NS
    return {
        "muestras": len(t),
        "media_ns": t.mean(),
        "sigma_ns": t.std(ddof=1) if len(t) > 1 else 0.0,
        "min_ns": t.min(),
        "max_ns": t.max(),
        "frecuencia_Hz": 1e9 / t.mean() if t.mean() > 0 else float("nan"),
    }


# ============================================================ decodificador de líneas ASCII

LINEA_ASCII = re.compile(rb"(?m)^(\d{1,5}),(\d{1,5})\r?\n")   # "I,Q" al inicio de una línea


class DecoAscii:
    """Junta los bytes que llegan y saca de cada línea completa "2048,2051\r\n" (I,Q) los
    códigos de I y Q (0 a 4095). Una línea que llega cortada o con otra forma no se usa y se
    cuenta en lineas_malas (es la única revisión que hay: el texto no lleva suma de verificación)."""

    def __init__(self):
        self.texto = bytearray()
        self.i, self.q = [], []
        self.lineas_malas = 0       # líneas que no tienen la forma número,número
        self.t_primera = None       # hora de la primera muestra (para medir muestras/s)

    def alimentar(self, datos):
        """Agrega bytes y devuelve la lista de (i, q) nuevos."""
        self.texto += datos
        nuevos = []
        fin = self.texto.rfind(b"\n")
        if fin < 0:
            if len(self.texto) > 4096:        # mucho sin un salto de línea: es basura
                self.lineas_malas += 1
                self.texto.clear()
            return nuevos
        bloque = bytes(self.texto[:fin + 1])
        del self.texto[:fin + 1]
        for m in LINEA_ASCII.finditer(bloque):
            i, q = int(m.group(1)), int(m.group(2))
            self.i.append(i)
            self.q.append(q)
            nuevos.append((i, q))
        total = bloque.count(b"\n")
        self.lineas_malas += max(0, total - len(nuevos) - bloque.count(b"Micro3"))
        if nuevos and self.t_primera is None:
            self.t_primera = time.perf_counter()
        return nuevos


def fs_desde_tasa(tasa):
    """El PIC muestrea a 1 kHz y manda 1 de cada k pares (k = ADC_PARES_POR_LINEA en
    adc_ascii.h; con 2 son 500 líneas/s). Se elige el 1000/k más cercano a lo medido."""
    if tasa <= 0:
        return FS
    k = FS_ADC / tasa
    for redondo in (1, 2, 3, 4, 5, 10, 20, 25, 50, 100, 200, 250):
        if abs(k - redondo) <= 0.04 * redondo:   # a tasas bajas la medida varía unos %
            return FS_ADC / redondo
    return FS_ADC / max(1, round(k))


# ============================================================ simulador (prueba sin PIC)

class Simulador:
    """Imita al PIC: muestrea a 1 kHz y manda una línea "I,Q\r\n" cada `cada` pares
    (2 = 500 líneas/s, como adc_ascii.c).
    modo "senoidal": I y Q senoidales de 1,7 V de amplitud, ruido de 0,5 LSB rms y un 3.er
    armónico de −75 dBc (el SINAD esperado es ≈ 66 dB).  modo "continua": 1,85 V con ruido."""

    def __init__(self, modo="senoidal", f=20.0, amp_V=1.7, ruido_LSB=0.5, cada=2):
        self.modo, self.f, self.amp, self.ruido = modo, f, amp_V / LSB, ruido_LSB
        self.cada = cada
        self.salida = bytearray(b"Micro3 listo\r\n")
        self.n = 0
        self.t0 = time.perf_counter()
        self.lock = threading.Lock()
        self.rng = np.random.default_rng(1)

    def write(self, datos):
        return len(datos)            # el PIC no recibe comandos

    def _muestra(self, t, fase):
        if self.modo == "continua":
            v = 2048.0
        else:
            a = 2 * math.pi * self.f * t + fase
            v = 2048 + self.amp * math.sin(a) + self.amp * 10 ** (-75 / 20) * math.sin(3 * a)
        v += self.rng.normal(0, self.ruido)
        return int(min(max(round(v), 0), 4095))

    def _generar(self):
        objetivo = int((time.perf_counter() - self.t0) * FS_ADC)
        while self.n < objetivo:
            if self.n % self.cada == 0:
                t = self.n / FS_ADC
                i, q = self._muestra(t, 0.0), self._muestra(t, math.pi / 2)
                self.salida += f"{i},{q}\r\n".encode()
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
            d = bytes(self.salida[:n])
            del self.salida[:n]
            return d

    def reset_input_buffer(self):
        with self.lock:
            self.salida.clear()

    def close(self):
        pass


# ============================================================ interfaz

PUERTO_SIM_SEN = "SIMULADOR (senoidal 20 Hz)"
PUERTO_SIM_DC = "SIMULADOR (continua 1,85 V)"


class App:
    def __init__(self, raiz):
        self.raiz = raiz
        raiz.title("Micro 3 — Receptor y mediciones del ADC (líneas ASCII I,Q)")
        raiz.geometry("1350x820")
        self.deco = None
        self.puerto = None
        self.hilo = None
        self.parar = threading.Event()
        self.t_inicio = None
        self.fs = FS
        self.ultimo = None          # último resultado de análisis
        self.ultimo_tipo = None

        self._armar_controles()
        self._armar_graficas()
        self.raiz.protocol("WM_DELETE_WINDOW", self.cerrar)
        self._refrescar_puertos()
        self._tick_auto = 0
        self.raiz.after(200, self._actualizar)

    # ---------------------------------------------------------------- panel izquierdo
    def _armar_controles(self):
        izq = ttk.Frame(self.raiz, padding=8)
        izq.pack(side=tk.LEFT, fill=tk.Y)

        f = ttk.LabelFrame(izq, text="1. Conexión", padding=8)
        f.pack(fill=tk.X, pady=4)
        ttk.Label(f, text="Puerto:").grid(row=0, column=0, sticky="w")
        self.cb_puerto = ttk.Combobox(f, width=28, state="readonly")
        self.cb_puerto.grid(row=0, column=1, columnspan=2, sticky="we", pady=2)
        ttk.Button(f, text="Refrescar", command=self._refrescar_puertos).grid(row=1, column=1, sticky="we")
        self.bt_conectar = ttk.Button(f, text="Conectar", command=self._conectar)
        self.bt_conectar.grid(row=1, column=2, sticky="we")

        f = ttk.LabelFrame(izq, text="2. Estado del enlace", padding=8)
        f.pack(fill=tk.X, pady=4)
        self.var_estado = tk.StringVar(value="Desconectado")
        ttk.Label(f, textvariable=self.var_estado, width=40, justify=tk.LEFT).pack(anchor="w")

        f = ttk.LabelFrame(izq, text="3. Medición", padding=8)
        f.pack(fill=tk.X, pady=4)
        ttk.Label(f, text="Canal:").grid(row=0, column=0, sticky="w")
        self.var_canal = tk.StringVar(value="I")
        ttk.Radiobutton(f, text="I (RC0)", variable=self.var_canal, value="I").grid(row=0, column=1, sticky="w")
        ttk.Radiobutton(f, text="Q (RC1)", variable=self.var_canal, value="Q").grid(row=0, column=2, sticky="w")
        ttk.Label(f, text="Tipo:").grid(row=1, column=0, sticky="nw")
        self.var_tipo = tk.StringVar(value="senoidal")
        for k, (val, txt) in enumerate([("senoidal", "Senoidal → SINAD, ENOB, THD"),
                                        ("continua", "Continua → ruido e histograma"),
                                        ("periodo", "Periodo (MODO_PERIODO en Q)")]):
            ttk.Radiobutton(f, text=txt, variable=self.var_tipo, value=val).grid(
                row=1 + k, column=1, columnspan=2, sticky="w")
        ttk.Label(f, text="Muestras:").grid(row=4, column=0, sticky="w", pady=(6, 0))
        self.cb_n = ttk.Combobox(f, width=8, state="readonly",
                                 values=["1024", "2048", "4096", "8192", "10000", "16384"])
        self.cb_n.set("4096")
        self.cb_n.grid(row=4, column=1, sticky="w", pady=(6, 0))
        ttk.Label(f, text="f entrada (Hz):").grid(row=5, column=0, sticky="w")
        self.en_f = ttk.Entry(f, width=8)
        self.en_f.grid(row=5, column=1, sticky="w")
        ttk.Label(f, text="vacío = automática").grid(row=5, column=2, sticky="w")
        self.var_auto = tk.BooleanVar(value=False)
        ttk.Checkbutton(f, text="Analizar cada 1 s", variable=self.var_auto).grid(
            row=6, column=0, columnspan=3, sticky="w", pady=(6, 0))
        ttk.Button(f, text="Analizar ahora", command=self.analizar).grid(
            row=7, column=0, columnspan=3, sticky="we", pady=(6, 0))

        f = ttk.LabelFrame(izq, text="4. Guardar", padding=8)
        f.pack(fill=tk.X, pady=4)
        ttk.Button(f, text="Guardar CSV de muestras", command=self.guardar_csv).pack(fill=tk.X)
        ttk.Button(f, text="Guardar reporte de la medición", command=self.guardar_reporte).pack(fill=tk.X, pady=2)
        ttk.Button(f, text="Borrar muestras", command=self.borrar).pack(fill=tk.X)

        f = ttk.LabelFrame(izq, text="Resultados", padding=8)
        f.pack(fill=tk.BOTH, expand=True, pady=4)
        barra = ttk.Scrollbar(f, orient=tk.VERTICAL)
        self.txt = tk.Text(f, width=46, height=16, font=("Consolas", 10), relief=tk.FLAT,
                           background=self.raiz.cget("background"), yscrollcommand=barra.set)
        barra.config(command=self.txt.yview)
        barra.pack(side=tk.RIGHT, fill=tk.Y)
        self.txt.pack(fill=tk.BOTH, expand=True)
        self.txt.tag_configure("ok", foreground="#1a7f37", font=("Consolas", 11, "bold"))
        self.txt.tag_configure("mal", foreground="#c62828", font=("Consolas", 11, "bold"))
        self.txt.tag_configure("titulo", font=("Consolas", 10, "bold"))
        self._escribir([("Conéctate y presiona «Analizar ahora».\n", None)])

    # ---------------------------------------------------------------- gráficas
    def _armar_graficas(self):
        der = ttk.Notebook(self.raiz)
        der.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=4, pady=4)
        self.figs = {}
        for nombre in ["Señal en vivo", "Espectro", "Histograma", "Plano I-Q"]:
            marco = ttk.Frame(der)
            der.add(marco, text=nombre)
            fig = Figure(figsize=(8, 6), dpi=100)
            ax = fig.add_subplot(111)
            lienzo = FigureCanvasTkAgg(fig, master=marco)
            NavigationToolbar2Tk(lienzo, marco).update()
            lienzo.get_tk_widget().pack(fill=tk.BOTH, expand=True)
            self.figs[nombre] = (fig, ax, lienzo)
        fig, ax, _ = self.figs["Señal en vivo"]
        self.linea_i, = ax.plot([], [], label="I", lw=0.9)
        self.linea_q, = ax.plot([], [], label="Q", lw=0.9)
        ax.set_xlabel("tiempo (s)")
        ax.set_ylabel("voltios")
        ax.set_ylim(0, VREF)
        ax.grid(True)
        ax.legend(loc="upper right")
        ax.set_title("Últimos 2 s (500 muestras/s)")
        for nombre in ["Espectro", "Histograma", "Plano I-Q"]:
            fig, ax, lienzo = self.figs[nombre]
            ax.text(0.5, 0.5, "Presiona «Analizar ahora»", ha="center", va="center", transform=ax.transAxes)
            lienzo.draw()

    # ---------------------------------------------------------------- conexión
    def _refrescar_puertos(self):
        try:
            from serial.tools import list_ports
            reales = [p.device for p in list_ports.comports()]
        except Exception:
            reales = []
        valores = reales + [PUERTO_SIM_SEN, PUERTO_SIM_DC]
        self.cb_puerto["values"] = valores
        if not self.cb_puerto.get():
            self.cb_puerto.set(valores[0])

    def _conectar(self):
        if self.puerto is not None:
            self._desconectar()
            return
        nombre = self.cb_puerto.get()
        try:
            if nombre == PUERTO_SIM_SEN:
                self.puerto = Simulador("senoidal")
            elif nombre == PUERTO_SIM_DC:
                self.puerto = Simulador("continua")
            else:
                import serial
                self.puerto = serial.Serial(nombre, BAUD, bytesize=8, parity="N", stopbits=1, timeout=0.05)
        except Exception as e:
            self.puerto = None
            messagebox.showerror("No se pudo abrir el puerto",
                                 f"{e}\n\n¿Otro programa (LabVIEW, Arduino IDE) tiene abierto el COM?")
            return
        self.deco = DecoAscii()
        self.fs = FS
        self.parar.clear()
        time.sleep(0.1)
        self.puerto.reset_input_buffer()
        self.t_inicio = time.perf_counter()
        self.hilo = threading.Thread(target=self._leer, daemon=True)
        self.hilo.start()
        self.bt_conectar.config(text="Desconectar")
        self.cb_puerto.config(state="disabled")

    def _leer(self):
        while not self.parar.is_set():
            try:
                n = self.puerto.in_waiting
                datos = self.puerto.read(n if n else 1)
            except Exception:
                self.parar.set()
                break
            if datos:
                self.deco.alimentar(datos)

    def _desconectar(self):
        self.parar.set()
        if self.hilo:
            self.hilo.join(timeout=1)
        try:
            self.puerto.close()
        except Exception:
            pass
        self.puerto = None
        self.bt_conectar.config(text="Conectar")
        self.cb_puerto.config(state="readonly")

    # ---------------------------------------------------------------- ciclo de la interfaz
    def _actualizar(self):
        d = self.deco
        if d is not None:
            n = len(d.i)
            ahora = time.perf_counter()
            dur = ahora - self.t_inicio if self.t_inicio else 0
            aviso = ""
            if n == 0 and dur > 1.5:
                aviso = ("\n⚠ No llega nada: revisa el COM, JP_TX/JP_RX\n"
                         "   y que el PIC esté programado.")
            # muestras/s medidas desde la primera muestra
            dt = ahora - d.t_primera if d.t_primera else 0
            tasa = (n - 1) / dt if dt > 0.5 and n > 1 else 0
            if n > 50 and dt > 2:
                self.fs = fs_desde_tasa(tasa)
                if self.fs < 100:
                    aviso += (f"\n⚠ Solo {self.fs:.0f} líneas/s: para SINAD pon\n"
                              "   ADC_PARES_POR_LINEA 2 en adc_ascii.h\n"
                              "   (500 líneas/s) y vuelve a programar.")
            linea_cod = ""
            if n:
                ui, uq = d.i[-1], d.q[-1]
                m = min(n, max(int(self.fs), 50))
                bi, bq = d.i[-m:], d.q[-m:]
                linea_cod = (f"\núltimo  I = {ui} ({ui * LSB:.3f} V)   Q = {uq} ({uq * LSB:.3f} V)"
                             f"\nmín/máx I {min(bi)}…{max(bi)}   Q {min(bq)}…{max(bq)}")
                if max(max(bi), max(bq)) > 4095:
                    aviso += ("\n⚠ Llegan códigos > 4095: revisa que el ADC\n"
                              "   esté justificado a la derecha (ADFM = 1).")
                for nom, b in (("I", bi), ("Q", bq)):
                    if min(b) >= 4070:
                        aviso += (f"\n⚠ {nom} pegado a 4095 (3,7 V = VCC): el pin\n"
                                  "   está en VDD. Mide el pin con el multímetro.")
                    elif max(b) <= 25:
                        aviso += (f"\n⚠ {nom} ≈ 0 V: no llega señal al pin.\n"
                                  "   Revisa J2/J7, JP_IN y JP_ADC.")
            estado = "recibiendo" if n else "esperando líneas I,Q"
            self.var_estado.set(
                f"{estado}\nmuestras: {n}   ({tasa:.0f} /s → fs = {self.fs:.0f} Hz)\n"
                f"líneas malas: {d.lineas_malas}" + linea_cod + aviso)
            if n:
                k = min(n, int(2 * self.fs))
                iv = np.asarray(d.i[-k:], float) * LSB
                qv = np.asarray(d.q[-k:], float) * LSB
                t = np.arange(n - k, n) / self.fs
                self.linea_i.set_data(t, iv)
                self.linea_q.set_data(t, qv)
                fig, ax, lienzo = self.figs["Señal en vivo"]
                ax.set_xlim(t[0], t[0] + 2.0)
                ax.set_title(f"Últimos 2 s ({self.fs:.0f} muestras/s)")
                lo = min(iv.min(), qv.min())
                hi = max(iv.max(), qv.max())
                if self.var_tipo.get() == "periodo" or lo < -0.01 or hi > VREF + 0.01:
                    margen = max(0.05 * (hi - lo), 0.01)
                    ax.set_ylim(lo - margen, hi + margen)
                else:
                    ax.set_ylim(-0.05, VREF + 0.05)
                lienzo.draw_idle()
            if self.var_auto.get():
                self._tick_auto += 1
                if self._tick_auto >= 5:
                    self._tick_auto = 0
                    self.analizar(silencioso=True)
        self.raiz.after(200, self._actualizar)

    # ---------------------------------------------------------------- análisis
    def _datos(self):
        if self.deco is None:
            return None, "Primero conéctate."
        n = int(self.cb_n.get())
        datos = self.deco.i if self.var_canal.get() == "I" else self.deco.q
        if len(datos) < n:
            return None, f"Faltan muestras: hay {len(datos)} de {n} ({(n - len(datos)) / self.fs:.1f} s más)."
        return np.asarray(datos[-n:], dtype=float), None

    def analizar(self, silencioso=False):
        x, error = self._datos()
        if error:
            if not silencioso:
                self._escribir([(error + "\n", "mal")])
            return
        tipo = self.var_tipo.get()
        canal = self.var_canal.get()
        try:
            if tipo == "senoidal":
                f_txt = self.en_f.get().strip().replace(",", ".")
                r = analizar_senoidal(x, fs=self.fs, f_in=float(f_txt) if f_txt else None)
                self._mostrar_senoidal(r, canal)
            elif tipo == "continua":
                r = analizar_continua(x)
                self._mostrar_continua(r, canal, x)
            else:
                r = analizar_periodo(x)
                self._mostrar_periodo(r, x)
        except Exception as e:
            self._escribir([(f"Error en el análisis: {e}\n", "mal")])
            return
        self.ultimo, self.ultimo_tipo = r, tipo
        self._plano_iq()

    def _mostrar_senoidal(self, r, canal):
        bien = r["sinad_dB"] >= SINAD_MIN
        lineas = [
            (f"SENOIDAL — canal {canal} — {r['muestras']} muestras\n\n", "titulo"),
            (f"SINAD   {r['sinad_dB']:6.1f} dB   ", None),
            ("CUMPLE ≥ 60 dB\n" if bien else "NO CUMPLE (< 60 dB)\n", "ok" if bien else "mal"),
            (f"ENOB    {r['enob_bits']:6.2f} bits\n", None),
            (f"SNR     {r['snr_dB']:6.1f} dB\n", None),
            (f"THD     {r['thd_dB']:6.1f} dB  (armónicos 2 a 5)\n", None),
            (f"SFDR    {r['sfdr_dB']:6.1f} dB\n\n", None),
            (f"Frecuencia   {r['frecuencia_Hz']:8.3f} Hz ({r['ciclos']:.1f} ciclos)\n", None),
            (f"Amplitud     {r['amplitud_pico_V']:8.4f} V pico\n", None),
            (f"Continua     {r['dc_V']:8.4f} V  (código {r['dc_codigo']:.1f})\n", None),
            (f"Ruido+dist.  {r['ruido_dist_rms_mV']:8.3f} mV rms ({r['ruido_dist_rms_LSB']:.2f} LSB)\n", None),
            (f"Mín / máx    {r['minimo']} / {r['maximo']}\n\n", None),
            (f"Referido a escala completa (1,85 V pico):\n", None),
            (f"SINAD_FS {r['sinad_fs_dB']:5.1f} dB   ENOB_FS {r['enob_fs_bits']:.2f} bits\n", None),
            ("\nIdeal 12 bits: 74,0 dB. Requisito: > 60 dB\n(ENOB > 9,67; ruido+dist. < 1,31 mV rms).\n", None),
        ]
        if r["pocos_ciclos"]:
            lineas.append(("\n⚠ Muy pocos ciclos en la captura: la fundamental\n   se mezcla con la continua. Usa más muestras\n   o una frecuencia mayor (≥ 2 Hz con 4096 a 500 Hz).\n", "mal"))
        if r["recorte"]:
            lineas.append(("\n⚠ La señal toca 0 o 4095: hay RECORTE.\n   Baja la amplitud del generador.\n", "mal"))
        if r["frecuencia_Hz"] < 1.0:
            lineas.append(("\n⚠ No se ve una senoidal clara.\n   ¿Es una señal continua? Usa «Continua».\n", "mal"))
        self._escribir(lineas)

        fig, ax, lienzo = self.figs["Espectro"]
        ax.clear()
        esp = r["_espectro"]
        f = np.arange(len(esp)) * self.fs / r["muestras"]
        dbfs = 10 * np.log10(np.maximum(esp, 1e-30) / r["_p_fs"])
        ax.plot(f, dbfs, lw=0.7, color="#1f4e79")
        lo, hi = r["_banda"]
        ax.plot(f[lo:hi], dbfs[lo:hi], lw=1.4, color="#2e7d32", label="fundamental")
        for h, kh, _p in r["_armonicos"]:
            ax.plot(f[kh], dbfs[kh], "v", color="#c62828")
            ax.annotate(f"{h}", (f[kh], dbfs[kh]), textcoords="offset points", xytext=(0, 6),
                        ha="center", fontsize=8, color="#c62828")
        ax.set_xlim(0, self.fs / 2)
        ax.set_ylim(max(dbfs.min(), -160) - 5, 5)
        ax.set_xlabel("frecuencia (Hz)")
        ax.set_ylabel("dBFS (0 dB = senoidal a escala completa)")
        ax.set_title(f"Espectro canal {canal}: SINAD = {r['sinad_dB']:.1f} dB, ENOB = {r['enob_bits']:.2f} bits")
        ax.grid(True)
        ax.legend(loc="upper right")
        lienzo.draw()
        self._histograma(np.asarray((self.deco.i if canal == "I" else self.deco.q)[-r["muestras"]:]), canal,
                         gauss=False)

    def _mostrar_continua(self, r, canal, x):
        lineas = [
            (f"CONTINUA — canal {canal} — {r['muestras']} muestras\n\n", "titulo"),
            (f"Media          {r['media_codigo']:9.2f} códigos = {r['media_V']:.4f} V\n", None),
            (f"Ruido rms (σ)  {r['sigma_LSB']:9.3f} LSB = {r['sigma_uV']:.1f} µV\n", None),
            (f"Pico a pico    {r['pp_codigos']:9d} códigos (medido)\n", None),
            (f"Pico a pico    {r['pp_estimado_uV']:9.1f} µV (≈ 6,6 σ)\n", None),
            (f"Códigos dist.  {r['codigos_distintos']:9d}\n", None),
            (f"Resol. efect.  {r['resol_efectiva_bits']:9.2f} bits\n", None),
            (f"Libre de ruido {r['resol_libre_ruido_bits']:9.2f} bits\n", None),
            (f"Incert. de σ   {r['incertidumbre_sigma_pct']:9.2f} %\n\n", None),
            ("Ideal (solo cuantización): σ = 0,29 LSB\n= 261 µV. Criterio propuesto: σ ≤ 1,4 LSB.\n", None),
        ]
        bien = r["sigma_LSB"] <= 1.4
        lineas.insert(3, ("   → CUMPLE σ ≤ 1,4 LSB\n" if bien else "   → NO CUMPLE σ ≤ 1,4 LSB\n",
                          "ok" if bien else "mal"))
        self._escribir(lineas)
        self._histograma(x, canal, gauss=True, r=r)

    def _mostrar_periodo(self, r, x):
        lineas = [
            (f"PERIODO — {r['muestras']} muestras (Q = cuentas de Timer1)\n\n", "titulo"),
            (f"Media      {r['media_ns']:12.1f} ns\n", None),
            (f"σ          {r['sigma_ns']:12.1f} ns\n", None),
            (f"Mín / máx  {r['min_ns']:.1f} / {r['max_ns']:.1f} ns\n", None),
            (f"Frecuencia {r['frecuencia_Hz']:12.3f} Hz\n\n", None),
            ("Resolución: 62,5 ns por cuenta. Solo tiene\nsentido con MODO_PERIODO = 1 en el firmware.\n", None),
        ]
        self._escribir(lineas)
        fig, ax, lienzo = self.figs["Histograma"]
        ax.clear()
        t = x * T_TIMER1_NS
        ax.hist(t, bins=min(100, max(10, len(np.unique(x)))), color="#1f4e79")
        ax.set_xlabel("periodo (ns)")
        ax.set_ylabel("ocurrencias")
        ax.set_title(f"Histograma del periodo: media {r['media_ns']:.0f} ns, σ = {r['sigma_ns']:.1f} ns")
        ax.grid(True)
        lienzo.draw()

    def _histograma(self, x, canal, gauss, r=None):
        fig, ax, lienzo = self.figs["Histograma"]
        ax.clear()
        x = np.asarray(x, dtype=float)
        lo, hi = int(x.min()), int(x.max())
        if hi - lo > 400:
            bins = 200
        else:
            bins = np.arange(lo, hi + 2) - 0.5
        ax.hist(x, bins=bins, color="#1f4e79", edgecolor="white", linewidth=0.3)
        if gauss and r and r["sigma_LSB"] > 0:
            xs = np.linspace(lo - 1, hi + 1, 400)
            g = len(x) / (r["sigma_LSB"] * math.sqrt(2 * math.pi)) * np.exp(
                -0.5 * ((xs - r["media_codigo"]) / r["sigma_LSB"]) ** 2)
            ax.plot(xs, g, color="#c62828", lw=1.2, label="gaussiana con la misma σ")
            ax.legend(loc="upper right")
            ax.set_title(f"Histograma de códigos, canal {canal}: σ = {r['sigma_LSB']:.2f} LSB")
        else:
            ax.set_title(f"Histograma de códigos, canal {canal}")
        ax.set_xlabel("código del ADC")
        ax.set_ylabel("ocurrencias")
        ax.grid(True)
        lienzo.draw()

    def _plano_iq(self):
        n = int(self.cb_n.get())
        fig, ax, lienzo = self.figs["Plano I-Q"]
        ax.clear()
        iv = np.asarray(self.deco.i[-n:], float) * LSB
        qv = np.asarray(self.deco.q[-n:], float) * LSB
        ax.plot(iv, qv, ".", ms=2, color="#1f4e79")
        ax.set_xlim(0, VREF)
        ax.set_ylim(0, VREF)
        ax.set_aspect("equal")
        ax.set_xlabel("I (V)")
        ax.set_ylabel("Q (V)")
        ax.set_title(f"Plano I-Q (últimas {len(iv)} muestras)")
        ax.grid(True)
        lienzo.draw()

    # ---------------------------------------------------------------- texto y archivos
    def _escribir(self, partes):
        self.txt.config(state=tk.NORMAL)
        self.txt.delete("1.0", tk.END)
        for texto, tag in partes:
            self.txt.insert(tk.END, texto, tag)
        self.txt.config(state=tk.DISABLED)

    def guardar_csv(self):
        if not self.deco or not self.deco.i:
            messagebox.showinfo("Sin datos", "No hay muestras todavía.")
            return
        ruta = filedialog.asksaveasfilename(defaultextension=".csv", initialfile="captura_micro3.csv",
                                            filetypes=[("CSV", "*.csv")])
        if not ruta:
            return
        with open(ruta, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["n", "t_s", "i", "q", "i_V", "q_V"])
            for k, (i, q) in enumerate(zip(self.deco.i, self.deco.q)):
                w.writerow([k, f"{k / self.fs:.4f}", i, q, f"{i * LSB:.5f}", f"{q * LSB:.5f}"])
        messagebox.showinfo("Guardado", f"{len(self.deco.i)} muestras en\n{ruta}")

    def guardar_reporte(self):
        if self.ultimo is None:
            messagebox.showinfo("Sin medición", "Primero presiona «Analizar ahora».")
            return
        ruta = filedialog.asksaveasfilename(defaultextension=".txt", initialfile="reporte_medicion.txt",
                                            filetypes=[("Texto", "*.txt")])
        if not ruta:
            return
        with open(ruta, "w", encoding="utf-8") as f:
            f.write(f"Micro 3 — medición {self.ultimo_tipo}, canal {self.var_canal.get()}, "
                    f"{time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"fs = {self.fs:.0f} muestras/s (líneas ASCII)\n")
            f.write(self.txt.get("1.0", tk.END))
            f.write("\nValores numéricos:\n")
            for k, v in self.ultimo.items():
                if not k.startswith("_"):
                    f.write(f"  {k} = {v}\n")
        for nombre, (fig, _ax, _l) in self.figs.items():
            fig.savefig(ruta.rsplit(".", 1)[0] + "_" + nombre.replace(" ", "_").replace("ñ", "n") + ".png", dpi=150)
        messagebox.showinfo("Guardado", f"Reporte y gráficas guardados junto a\n{ruta}")

    def borrar(self):
        if self.deco:
            d = self.deco
            d.i.clear(); d.q.clear()
            d.lineas_malas = 0; d.t_primera = None
            self.t_inicio = time.perf_counter()

    def cerrar(self):
        if self.puerto is not None:
            self._desconectar()
        self.raiz.destroy()


def main():
    raiz = tk.Tk()
    app = App(raiz)
    if "--autotest" in sys.argv:            # prueba automática (sin teclado): simula, analiza y sale
        app.cb_puerto.set(PUERTO_SIM_SEN)
        app._conectar()

        def paso1():
            app.analizar()
            for nombre, (fig, _ax, _l) in app.figs.items():
                fig.savefig(f"autotest_{nombre.replace(' ', '_').replace('ñ', 'n')}.png", dpi=80)
            print(app.txt.get("1.0", tk.END))
            app.var_tipo.set("continua")
            app.analizar()
            print(app.txt.get("1.0", tk.END))
            app.figs["Histograma"][0].savefig("autotest_Histograma_continua.png", dpi=80)
            raiz.update()
            raiz.after(300, app.cerrar)
        raiz.after(6000, paso1)
    raiz.mainloop()


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        _falta("La interfaz se detuvo por un error:\n\n" + traceback.format_exc())
