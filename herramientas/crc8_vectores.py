"""Vectores de CRC-8 (polinomio 0x07) calculados en Python, independientes del PIC.
Uso: python crc8_vectores.py   -> imprime el CRC de "123456789" y de las 5 tramas de la
especificacion (docs/especificacion_protocolo.md, seccion 3)."""
def crc8(datos):
    crc = 0x00
    for b in datos:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc

if __name__ == "__main__":
    assert crc8(b"123456789") == 0xF4
    tramas = {"PING seq0": [1,0,0,0,0,0], "PING seq7": [1,7,0x11,0x22,0x33,0x44],
              "PONG seq7": [0x81,7,0x11,0x22,0x33,0x44], "ERROR 1": [0x7F,0,1,0,0,0],
              "TYPE 0x55": [0x55,3,0,0,0,0]}
    print("123456789 ->", hex(crc8(b"123456789")))
    for n, c in tramas.items():
        print(n, "->", hex(crc8(bytes(c))))
