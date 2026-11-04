function generar_diagramas_tiempo()
% GENERAR_DIAGRAMAS_TIEMPO  Diagramas de tiempo del ADC y la UART - Micro 3
%
% Genera 5 figuras (PNG) en la carpeta "figuras" junto a este archivo:
%   fig1_periodo_1ms.png   periodo de muestreo completo (1 ms)
%   fig2_rafaga_IQ.png     adquisicion y conversion de I y Q
%   fig3_armado_trama.png  conversion de datos y armado de la trama
%   fig4_byte_trama.png    un byte 8N1 y la trama de 9 bytes en la linea
%   fig5_ascii_binario.png comparacion ASCII vs binario
%
% Los tiempos de CPU son ESTIMADOS (conteo de instrucciones). Cuando se
% midan con el osciloscopio, cambiar los valores de la seccion 2 y volver
% a correr el script.
%
% Requiere MATLAB R2020a o mas nuevo (archivo en UTF-8). Tambien corre en
% GNU Octave 6 o mas nuevo.

close all;
set(0, 'DefaultTextInterpreter', 'none');
set(0, 'DefaultAxesTickLabelInterpreter', 'none');
carpeta = fullfile(fileparts(mfilename('fullpath')), 'figuras');
if ~exist(carpeta, 'dir')
    mkdir(carpeta);
end

%% 1. Datos del microcontrolador (PIC16F18426)
Fosc = 16e6;               % oscilador interno [Hz]
Tcy  = 4/Fosc;             % ciclo de instruccion = 250 ns
TAD  = 32/Fosc;            % reloj del ADC: Fosc/32 = 2 us
Tconv = 14*TAD + 2*Tcy;    % conversion de 12 bits (datasheet AD22) = 28,5 us

% Timer0: Fosc/4, prescaler 1:16, TMR0H = 249  ->  periodo de muestreo
Ts = (249+1) * 16 * Tcy;   % = 1 ms

% UART: BRGH = 1, BRG16 = 1, SP1BRG = 34
SPBRG  = 34;
baud   = Fosc/(4*(SPBRG+1));   % = 114 286 baud (nominal 115200)
Tbit   = 1/baud;               % = 8,75 us
Tbyte  = 10*Tbit;              % 8N1 = 10 bits = 87,5 us

%% 2. Tiempos (en microsegundos)
us = 1e6;
t_acq   = 10;            % adquisicion, caso A (ADACQ = 160, unidad 62,5 ns). Medir: prueba 4.16
t_conv  = Tconv*us;      % 28,5
t_lat   = 1;             % latencia de interrupcion 3-4 Tcy (datasheet)
t_go_q  = 6.5;           % desde que entra la ISR de I hasta ADGO = 1  [ESTIMADO]
t_isr_i = 10.5;          % duracion ISR de I  [ESTIMADO]
t_isr_q = 11.5;          % duracion ISR de Q  [ESTIMADO]
t_poll  = 3;             % main se da cuenta de que hay par nuevo  [ESTIMADO]
t_get   = 5;             % adc_iq_get()  [ESTIMADO]
t_split = 2.5;           % separar I y Q en bytes  [ESTIMADO]
t_head  = 7.5;           % cabecera A5 5A, TYPE, SEQ, payload  [ESTIMADO]
t_crc   = 150;           % CRC-8 bit a bit sobre 6 bytes  [ESTIMADO]
t_txdat = 50;            % tx_DAT: copiar 9 bytes al buffer  [ESTIMADO]
t_txisr = 6;             % ISR de TX, una por byte  [ESTIMADO]
t_bit   = Tbit*us;
t_byte  = Tbyte*us;
t_Ts    = Ts*us;

% instantes de la rafaga (desde el disparo del Timer0)
I_muestra = t_acq;
I_adif    = I_muestra + t_conv;
isrI_ini  = I_adif + t_lat;
isrI_fin  = isrI_ini + t_isr_i;
Q_go      = isrI_ini + t_go_q;
Q_muestra = Q_go + t_acq;
Q_adif    = Q_muestra + t_conv;
isrQ_ini  = Q_adif + t_lat;
isrQ_fin  = isrQ_ini + t_isr_q;
% main
m_ini  = isrQ_fin + t_poll;
m_get  = m_ini + t_get;
m_spl  = m_get + t_split;
m_head = m_spl + t_head;
m_crc  = m_head + t_crc;
m_dat  = m_crc + t_txdat;
tx_ini = m_dat + 1.5;
t_trama = 9*t_byte;

fprintf('\n--- Tiempos usados en las figuras ---\n');
fprintf('Periodo de muestreo         %8.1f us\n', t_Ts);
fprintf('Conversion por canal        %8.1f us\n', t_conv);
fprintf('Rafaga I+Q completa         %8.1f us\n', isrQ_fin);
fprintf('Sesgo muestra I -> Q        %8.1f us\n', Q_muestra - I_muestra);
fprintf('Armado de trama (main)      %8.1f us\n', m_dat - m_ini);
fprintf('Baud real                   %8.0f baud (error %.2f %%)\n', baud, (baud/115200-1)*100);
fprintf('Byte 8N1                    %8.2f us\n', t_byte);
fprintf('Trama 9 bytes               %8.1f us (%.1f %% del periodo)\n', t_trama, 100*t_trama/t_Ts);
fprintf('Linea ASCII 15 bytes        %8.1f us\n\n', 15*t_byte);

% colores (los de MATLAB por defecto)
azul    = [0 0.4470 0.7410];
naranja = [0.8500 0.3250 0.0980];
verde   = [0.4660 0.6740 0.1880];
morado  = [0.4940 0.1840 0.5560];
gris    = [0.6 0.6 0.6];
claro   = @(c) c + (1-c)*0.55;

%% Figura 1: un periodo completo
f = nueva_figura([100 100 1000 480]);
ax = gca; hold on;
filas = {'RC2 (marca ISR)', 'UART TX', 'main', 'Interrupciones', 'ADC', 'Timer0'};
% Timer0: pulso en cada coincidencia
senal_digital([0 0; 0 1; 1 0; 1000 1; 1001 0], 6, 1150);
% ADC
bloque(0, I_muestra, 5, claro(azul), '');
bloque(I_muestra, I_adif, 5, azul, 'I');
bloque(Q_go, Q_muestra, 5, claro(azul), '');
bloque(Q_muestra, Q_adif, 5, azul, 'Q');
bloque(1000, 1000+I_muestra, 5, claro(azul), '');
bloque(1000+I_muestra, 1000+I_adif, 5, azul, 'I');
bloque(1000+Q_go, 1000+Q_muestra, 5, claro(azul), '');
bloque(1000+Q_muestra, 1000+Q_adif, 5, azul, 'Q');
% interrupciones
bloque(isrI_ini, isrI_fin, 4, naranja, '');
bloque(isrQ_ini, isrQ_fin, 4, naranja, '');
bloque(1000+isrI_ini, 1000+isrI_fin, 4, naranja, '');
bloque(1000+isrQ_ini, 1000+isrQ_fin, 4, naranja, '');
t_isr_tx = [m_dat, m_dat+7, tx_ini + (1:8)*t_byte];
for k = 1:numel(t_isr_tx)
    bloque(t_isr_tx(k), t_isr_tx(k)+t_txisr, 4, morado, '');
end
text(isrQ_fin+8, 4.45, 'ISR ADC', 'Color', naranja, 'FontSize', 11);
text(t_isr_tx(3)+8, 4.45, 'ISR TX (1 por byte)', 'Color', morado, 'FontSize', 11);
% main
bloque(m_ini, m_head, 3, claro(verde), '');
bloque(m_head, m_crc, 3, verde, 'CRC-8');
bloque(m_crc, m_dat, 3, verde, 'tx');
text(m_dat+20, 3, 'protocol_task() (libre)', 'FontSize', 11, 'Color', [0.4 0.4 0.4]);
% UART
nombres = {'A5','5A','A1','SEQ','P0','P1','P2','P3','CRC'};
for k = 1:9
    bloque(tx_ini+(k-1)*t_byte, tx_ini+k*t_byte, 2, claro(morado), nombres{k});
end
cota(tx_ini, tx_ini+t_trama, 1.42, sprintf('trama 9 bytes = %.1f µs (%.0f %% del periodo)', t_trama, 100*t_trama/t_Ts));
% RC2
senal_digital([0 0; isrI_ini+2.5 1; isrI_fin-1.5 0; isrQ_ini+2.5 1; isrQ_fin-1.5 0; ...
               1000+isrI_ini+2.5 1; 1000+isrI_fin-1.5 0; 1000+isrQ_ini+2.5 1; 1000+isrQ_fin-1.5 0], 1, 1150);
cota(0, 1000, 6.6, sprintf('Ts = %.0f µs', t_Ts));
plot([1000 1000], [0.5 6.9], 'k:');
terminar(ax, filas, [-10 1150], [0.5 7], ...
    'Figura 1. Periodo de muestreo completo (1 kHz, sin promedio, ADACQ caso A)');
guardar(f, carpeta, 'fig1_periodo_1ms');

%% Figura 2: rafaga de I y Q
f = nueva_figura([100 100 1000 440]);
ax = gca; hold on;
filas = {'RC2', 'ADPCH', 'Interrupción', 'ADC', 'Timer0'};
senal_digital([-5 0; 0 1; 1 0], 5, 110);
bloque(0, I_muestra, 4, claro(azul), 'adq.');
bloque(I_muestra, I_adif, 4, azul, sprintf('conversión I (%.1f µs)', t_conv));
bloque(Q_go, Q_muestra, 4, claro(azul), 'adq.');
bloque(Q_muestra, Q_adif, 4, azul, sprintf('conversión Q (%.1f µs)', t_conv));
bloque(isrI_ini, isrI_fin, 3, naranja, 'ISR I');
bloque(isrQ_ini, isrQ_fin, 3, naranja, 'ISR Q');
bloque(-5, Q_go-1.5, 2, gris, 'canal I (RC0)');
bloque(Q_go-1.5, isrQ_ini+2, 2, [0.45 0.45 0.45], 'canal Q (RC1)');
bloque(isrQ_ini+2, 110, 2, gris, 'I');
senal_digital([-5 0; isrI_ini+2.5 1; isrI_fin-1.5 0; isrQ_ini+2.5 1; isrQ_fin-1.5 0], 1, 110);
plot([I_muestra I_muestra], [0.5 5.6], '--', 'Color', azul);
plot([Q_muestra Q_muestra], [0.5 5.6], '--', 'Color', azul);
text(I_muestra+0.5, 5.45, 'muestra I', 'Color', azul, 'FontSize', 11);
text(Q_muestra+0.5, 5.45, 'muestra Q', 'Color', azul, 'FontSize', 11);
cota(0, I_muestra, 4.5, sprintf('%.0f µs', t_acq));
cota(I_muestra, Q_muestra, 5.85, sprintf('sesgo I-Q = %.0f µs', Q_muestra - I_muestra));
cota(0, isrQ_fin, 0.2, sprintf('ráfaga completa = %.0f µs', isrQ_fin));
text(I_adif-0.5, 3, sprintf('latencia %.2g µs \->', t_lat), 'HorizontalAlignment', 'right', 'FontSize', 11);
terminar(ax, filas, [-5 110], [-0.1 6.3], 'Figura 2. Adquisición, conversión e interrupciones de I y Q');
guardar(f, carpeta, 'fig2_rafaga_IQ');

%% Figura 3: armado de la trama
f = nueva_figura([100 100 1000 380]);
ax = gca; hold on;
o = isrQ_ini;   % tiempo relativo al inicio de la ISR de Q
filas = {'UART TX', 'main', 'Interrupción'};
bloque(0, isrQ_fin-o, 3, naranja, '');
text(isrQ_fin-o+2, 3.42, 'ISR Q', 'Color', naranja, 'FontSize', 11);
bloque(m_ini-o, m_get-o, 2, claro(verde), '');
bloque(m_get-o, m_spl-o, 2, 0.5*(verde+claro(verde)), '');
bloque(m_spl-o, m_head-o, 2, claro(verde), '');
bloque(m_head-o, m_crc-o, 2, verde, sprintf('crc8() = %.0f µs', t_crc));
bloque(m_crc-o, m_dat-o, 2, verde, 'tx_DAT');
text(m_ini-o, 1.45, sprintf('adc_iq_get %.0f µs | separar bytes %.1f µs | cabecera %.1f µs', ...
     t_get, t_split, t_head), 'FontSize', 11);
for tt = [m_dat, m_dat+7, tx_ini+t_byte]
    bloque(tt-o, tt-o+t_txisr, 3, morado, '');
end
text(m_dat-o, 3.42, 'TX1IE = 1 -> sale el primer byte', 'Color', morado, 'FontSize', 11);
hex = {'A5', '5A', 'A1', '05'};
for k = 1:4
    bloque(tx_ini-o+(k-1)*t_byte, tx_ini-o+k*t_byte, 1, claro(morado), hex{k});
end
cota(m_ini-o, m_dat-o, 2.55, sprintf('armado de la trama = %.0f µs', m_dat - m_ini));
terminar(ax, filas, [-5 600], [0.5 3.8], 'Figura 3. Conversión de datos y armado de la trama DATA_IQ (I = 2048, Q = 2051)');
guardar(f, carpeta, 'fig3_armado_trama');

%% Figura 4: un byte y la trama
f = nueva_figura([100 100 1000 520]);
subplot(2,1,1); hold on;
bits = [0 1 0 1 0 0 1 0 1 1];   % inicio, 0xA5 con el LSB primero, parada
t = [-15, (0:9)*t_bit, 10*t_bit+15];
v = [1, bits, 1];
stairs(t, v, 'k', 'LineWidth', 1.5);
nom = {'inicio','D0','D1','D2','D3','D4','D5','D6','D7','parada'};
for k = 1:10
    text((k-0.5)*t_bit, 1.25, nom{k}, 'HorizontalAlignment', 'center', 'FontSize', 11);
    text((k-0.5)*t_bit, -0.25, num2str(bits(k)), 'HorizontalAlignment', 'center', 'FontSize', 12, 'FontWeight', 'bold');
    plot([(k-1) (k-1)]*t_bit, [-0.4 1.4], ':', 'Color', [0.7 0.7 0.7]);
end
ylim([-0.6 1.6]); xlim([-15 10*t_bit+15]);
set(gca, 'YTick', [0 1]);
grid on; box on;
xlabel('tiempo (µs)'); ylabel('nivel');
title(sprintf('Figura 4a. Byte 0xA5 en 8N1: bit = %.2f µs, byte = %.1f µs (%.0f baud)', t_bit, t_byte, baud));
subplot(2,1,2); hold on;
hex = {'A5','5A','A1','05','08','00','08','03','02'};
cam = {'sync','sync','TYPE','SEQ','I alto','I bajo','Q alto','Q bajo','CRC'};
for k = 1:9
    bloque((k-1)*t_byte, k*t_byte, 1, claro(morado), hex{k});
    text((k-0.5)*t_byte, 0.55, cam{k}, 'HorizontalAlignment', 'center', 'FontSize', 11);
end
cota(0, 9*t_byte, 1.45, sprintf('9 bytes = %.1f µs', 9*t_byte));
xlim([-10 9*t_byte+10]); ylim([0.3 1.8]);
set(gca, 'YTick', []);
grid on; box on;
xlabel('tiempo (µs)');
title('Figura 4b. Trama DATA_IQ (CRC-8 sobre los bytes 2 a 7)');
guardar(f, carpeta, 'fig4_byte_trama');

%% Figura 5: ASCII contra binario
t_dec = 95; t_arm = 10; t_len = 30; t_dat15 = 75;    % [ESTIMADO]
f = nueva_figura([100 100 1000 400]);
ax = gca; hold on;
filas = {'UART binario', 'main ASCII', 'UART ASCII', 'Timer0'};
senal_digital([0 0 ;0 1; 1 0; 1000 1; 1001 0; 2000 1; 2001 0], 4, 2250);
a = m_ini;
bloque(a, a+t_dec, 2, claro(verde), 'I');
bloque(a+t_dec, a+2*t_dec, 2, claro(verde), 'Q');
bloque(a+2*t_dec, a+2*t_dec+t_arm+t_len+t_dat15, 2, verde, 'tx');
a1 = a+2*t_dec+t_arm+t_len+t_dat15;
txt = {'I',':','2','0','4','8',',','Q',':','2','0','5','1','CR','LF'};
for k = 1:15
    bloque(a1+(k-1)*t_byte, a1+k*t_byte, 3, claro(morado), txt{k});
end
text(a1+15*t_byte+15, 3, sprintf('15 bytes = %.0f µs > Ts', 15*t_byte), 'FontSize', 11);
for p = [0 1000]
    for k = 1:9
        bloque(p+tx_ini+(k-1)*t_byte, p+tx_ini+k*t_byte, 1, claro(morado), '');
    end
end
text(tx_ini+4.5*t_byte, 1, sprintf('trama binaria %.0f µs', t_trama), 'HorizontalAlignment', 'center', 'FontSize', 11);
for p = [1000 2000]
    plot([p p], [0.5 4.5], 'k:');
end
text(a, 2.45, sprintf('a_decimal(I) + a_decimal(Q) + armar línea = %.0f µs', a1-a), 'FontSize', 11, 'Color', verde);
terminar(ax, filas, [-10 2250], [0.4 4.6], 'Figura 5. Línea ASCII (Serial Plotter) contra trama binaria');
guardar(f, carpeta, 'fig5_ascii_binario');

fprintf('Figuras guardadas en: %s\n', carpeta);
end

% ------------------------------------------------------------------------
function f = nueva_figura(pos)
f = figure('Color', 'w', 'Position', pos);
% MATLAB R2025a o más nuevo con tema oscuro: forzar tema claro para que los
% nombres de las filas y los ejes se vean (y salgan bien en el PNG).
try
    set(f, 'Theme', 'light');
catch
end
end

function bloque(x0, x1, y, color, texto)
% Rectangulo de x0 a x1 centrado en la fila y, con texto opcional.
h = 0.55;
rectangle('Position', [x0, y-h/2, x1-x0, h], 'FaceColor', color, 'EdgeColor', 'k', 'LineWidth', 0.5);
if ~isempty(texto)
    text((x0+x1)/2, y, texto, 'HorizontalAlignment', 'center', 'FontSize', 11, 'Clipping', 'on');
end
end

function senal_digital(puntos, y, tfin)
% Dibuja una senal digital en la fila y. puntos = [t nivel] (nivel 0 o 1).
t = [puntos(:,1); tfin];
v = [puntos(:,2); puntos(end,2)];
stairs(t, y - 0.25 + 0.5*v, 'k', 'LineWidth', 1.2);
end

function cota(x0, x1, y, texto)
% Linea de cota con marcas en los extremos y el texto encima.
plot([x0 x1], [y y], 'k-', 'LineWidth', 0.6);
plot([x0 x0], [y-0.08 y+0.08], 'k-', 'LineWidth', 0.6);
plot([x1 x1], [y-0.08 y+0.08], 'k-', 'LineWidth', 0.6);
text((x0+x1)/2, y+0.04, texto, 'HorizontalAlignment', 'center', 'VerticalAlignment', 'bottom', 'FontSize', 11);
end

function terminar(ax, filas, xl, yl, titulo)
set(ax, 'YTick', 1:numel(filas), 'YTickLabel', filas, 'FontSize', 12);
xlim(xl); ylim(yl);
grid on; box on;
xlabel('tiempo (µs)');
title(titulo, 'Interpreter', 'none');
end

function guardar(f, carpeta, nombre)
set(f, 'PaperPositionMode', 'auto');
print(f, fullfile(carpeta, [nombre '.png']), '-dpng', '-r200');
end
