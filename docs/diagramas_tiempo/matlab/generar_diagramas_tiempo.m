function generar_diagramas_tiempo()
% GENERAR_DIAGRAMAS_TIEMPO  Diagramas de tiempo del ADC y la UART - Micro 3
%
% El PIC muestrea I y Q a 1 kHz y manda una linea de texto
% "I:2048,Q:2051\r\n" cada 2 pares (500 lineas por segundo).
%
% Genera 5 figuras (PNG) en la carpeta "figuras" junto a este archivo:
%   fig1_periodo_2ms.png      dos periodos de muestreo y una linea en la UART
%   fig2_rafaga_IQ.png        adquisicion y conversion de I y Q
%   fig3_armado_linea.png     conversion a decimal y armado de la linea
%   fig4_byte_linea.png       un byte 8N1 y la linea de 15 bytes
%   fig5_ocupacion_linea.png  por que se manda 1 linea cada 2 pares
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
pares_por_linea = 2;           % ADC_PARES_POR_LINEA en adc_ascii.h

%% 2. Tiempos (en microsegundos)
us = 1e6;
t_acq   = 10;            % adquisicion, caso A (ADACQ = 160, unidad 62,5 ns). Medir: prueba 4.16
t_conv  = Tconv*us;      % 28,5
t_lat   = 1;             % latencia de interrupcion 3-4 Tcy (datasheet)
t_go_q  = 6.5;           % desde que entra la ISR de I hasta ADGO = 1  [ESTIMADO]
t_isr_i = 10.5;          % duracion ISR de I  [ESTIMADO]
t_isr_q = 11.5;          % duracion ISR de Q  [ESTIMADO]
t_poll  = 3;             % main se da cuenta de que hay par nuevo  [ESTIMADO]
t_get   = 5;             % adc_iq_get() + contar pares  [ESTIMADO]
t_dec   = 95;            % a_decimal() de un numero de 4 cifras  [ESTIMADO]
t_arm   = 10;            % poner 'I' ':' ',' 'Q' ':' CR LF  [ESTIMADO]
t_tx    = 105;           % tx_string(): strlen + copiar 15 bytes al buffer  [ESTIMADO]
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
% main (en el tick en que toca mandar la linea)
m_ini  = isrQ_fin + t_poll;
m_get  = m_ini + t_get;
m_decI = m_get + t_dec;
m_decQ = m_decI + t_dec;
m_arm  = m_decQ + t_arm;
m_tx   = m_arm + t_tx;
tx_ini = m_tx + 1.5;
n_lin  = 15;                    % "I:4095,Q:4095\r\n"
t_lin  = n_lin*t_byte;
T_lin  = pares_por_linea*t_Ts;  % una linea cada 2 ms

fprintf('\n--- Tiempos usados en las figuras ---\n');
fprintf('Periodo de muestreo         %8.1f us\n', t_Ts);
fprintf('Conversion por canal        %8.1f us\n', t_conv);
fprintf('Rafaga I+Q completa         %8.1f us\n', isrQ_fin);
fprintf('Sesgo muestra I -> Q        %8.1f us\n', Q_muestra - I_muestra);
fprintf('Armado de la linea (main)   %8.1f us\n', m_tx - m_ini);
fprintf('Baud real                   %8.0f baud (error %.2f %%)\n', baud, (baud/115200-1)*100);
fprintf('Byte 8N1                    %8.2f us\n', t_byte);
fprintf('Linea de 15 bytes           %8.1f us (%.1f %% de %.0f ms)\n', t_lin, 100*t_lin/T_lin, T_lin/1000);
fprintf('Una linea por par           %8.1f %% de la linea: no cabe\n\n', 100*t_lin/t_Ts);

% colores (los de MATLAB por defecto)
azul    = [0 0.4470 0.7410];
naranja = [0.8500 0.3250 0.0980];
verde   = [0.4660 0.6740 0.1880];
morado  = [0.4940 0.1840 0.5560];
rojo    = [0.6350 0.0780 0.1840];
gris    = [0.6 0.6 0.6];
claro   = @(c) c + (1-c)*0.55;
txt15   = {'I',':','2','0','4','8',',','Q',':','2','0','5','1','CR','LF'};

%% Figura 1: dos periodos de muestreo y una linea en la UART
f = nueva_figura([100 100 1000 480]);
ax = gca; hold on;
filas = {'RC2 (TMR0_out)', 'UART TX', 'main', 'Interrupciones', 'ADC', 'Timer0'};
tfin = 2200;
% Timer0: pulso en cada coincidencia
senal_digital([0 0; 0 1; 1 0; 1000 1; 1001 0; 2000 1; 2001 0], 6, tfin);
for p = [0 1000 2000]
    bloque(p, p+I_muestra, 5, claro(azul), '');
    bloque(p+I_muestra, p+I_adif, 5, azul, '');
    bloque(p+Q_go, p+Q_muestra, 5, claro(azul), '');
    bloque(p+Q_muestra, p+Q_adif, 5, azul, '');
    bloque(p+isrI_ini, p+isrI_fin, 4, naranja, '');
    bloque(p+isrQ_ini, p+isrQ_fin, 4, naranja, '');
end
text(Q_adif+15, 5, 'I y Q', 'Color', azul, 'FontSize', 11);
% ISR de TX: una por byte
t_isr_tx = [m_tx, tx_ini + (1:n_lin-1)*t_byte];
for k = 1:numel(t_isr_tx)
    bloque(t_isr_tx(k), t_isr_tx(k)+t_txisr, 4, morado, '');
end
text(isrQ_fin+8, 4.45, 'ISR ADC', 'Color', naranja, 'FontSize', 11);
text(t_isr_tx(4)+8, 4.45, 'ISR TX (1 por byte)', 'Color', morado, 'FontSize', 11);
% main: arma la linea en el tick 0; en el tick 1000 solo cuenta el par
bloque(m_ini, m_tx, 3, verde, 'línea');
bloque(1000+m_ini, 1000+m_get, 3, claro(verde), '');
text(1000+m_get+12, 3, 'solo cuenta el par', 'FontSize', 11, 'Color', [0.4 0.4 0.4]);
bloque(2000+m_ini, tfin, 3, verde, '');
% UART: 15 bytes
for k = 1:n_lin
    bloque(tx_ini+(k-1)*t_byte, tx_ini+k*t_byte, 2, claro(morado), txt15{k});
end
cota(tx_ini, tx_ini+t_lin, 1.42, sprintf('línea de 15 bytes = %.0f µs (%.0f %% de %.0f ms)', t_lin, 100*t_lin/T_lin, T_lin/1000));
% RC2 con ADC_PIN_PRUEBA = 2: TMR0_out cambia en cada coincidencia
senal_digital([0 0; 0 1; 1000 0; 2000 1], 1, tfin);
cota(0, 1000, 6.6, sprintf('Ts = %.0f µs', t_Ts));
plot([1000 1000], [0.5 6.9], 'k:');
plot([2000 2000], [0.5 6.9], 'k:');
terminar(ax, filas, [-10 tfin], [0.5 7], ...
    'Figura 1. Dos periodos de muestreo (1 kHz) y una línea ASCII cada 2 pares');
guardar(f, carpeta, 'fig1_periodo_2ms');

%% Figura 2: rafaga de I y Q
f = nueva_figura([100 100 1000 440]);
ax = gca; hold on;
filas = {'RC2 (TMR0_out)', 'ADPCH', 'Interrupción', 'ADC', 'Timer0'};
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
senal_digital([-5 0; 0 1], 1, 110);
plot([I_muestra I_muestra], [0.5 5.6], '--', 'Color', azul);
plot([Q_muestra Q_muestra], [0.5 5.6], '--', 'Color', azul);
text(I_muestra+0.5, 5.45, 'muestra I', 'Color', azul, 'FontSize', 11);
text(Q_muestra+0.5, 5.45, 'muestra Q', 'Color', azul, 'FontSize', 11);
cota(0, I_muestra, 4.5, sprintf('%.0f µs', t_acq));
cota(I_muestra, Q_muestra, 5.85, sprintf('sesgo I-Q = %.0f µs', Q_muestra - I_muestra));
cota(0, isrQ_fin, 0.2, sprintf('ráfaga completa = %.0f µs', isrQ_fin));
text(I_adif-0.5, 3, sprintf('latencia %.2g µs ->', t_lat), 'HorizontalAlignment', 'right', 'FontSize', 11);
terminar(ax, filas, [-5 110], [-0.1 6.3], 'Figura 2. Adquisición, conversión e interrupciones de I y Q');
guardar(f, carpeta, 'fig2_rafaga_IQ');

%% Figura 3: armado de la linea
f = nueva_figura([100 100 1000 380]);
ax = gca; hold on;
o = isrQ_ini;   % tiempo relativo al inicio de la ISR de Q
filas = {'UART TX', 'main', 'Interrupción'};
bloque(0, isrQ_fin-o, 3, naranja, '');
text(isrQ_fin-o+2, 3.42, 'ISR Q', 'Color', naranja, 'FontSize', 11);
bloque(m_ini-o, m_get-o, 2, claro(verde), '');
bloque(m_get-o, m_decI-o, 2, verde, 'a_decimal(I)');
bloque(m_decI-o, m_decQ-o, 2, verde, 'a_decimal(Q)');
bloque(m_decQ-o, m_arm-o, 2, claro(verde), '');
bloque(m_arm-o, m_tx-o, 2, 0.5*(verde+claro(verde)), 'tx_string');
text(m_ini-o, 1.45, sprintf('adc_iq_get %.0f µs | cada cifra: restar 1000, 100, 10, 1 y sumar ''0'' | texto fijo %.0f µs', ...
     t_get, t_arm), 'FontSize', 11);
for tt = [m_tx, tx_ini+t_byte, tx_ini+2*t_byte]
    bloque(tt-o, tt-o+t_txisr, 3, morado, '');
end
text(m_tx-o, 3.42, 'TX1IE = 1 -> sale el primer byte', 'Color', morado, 'FontSize', 11);
for k = 1:4
    bloque(tx_ini-o+(k-1)*t_byte, tx_ini-o+k*t_byte, 1, claro(morado), txt15{k});
end
cota(m_ini-o, m_tx-o, 2.55, sprintf('armado de la línea = %.0f µs', m_tx - m_ini));
terminar(ax, filas, [-5 700], [0.5 3.8], 'Figura 3. Conversión a decimal y armado de la línea "I:2048,Q:2051\r\n"');
guardar(f, carpeta, 'fig3_armado_linea');

%% Figura 4: un byte y la linea
f = nueva_figura([100 100 1000 540]);
subplot(2,1,1); hold on;
bits = [0 1 0 0 1 0 0 1 0 1];   % inicio, 'I' = 0x49 con el LSB primero, parada
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
title(sprintf('Figura 4a. Byte ''I'' (0x49) en 8N1: bit = %.2f µs, byte = %.1f µs (%.0f baud)', t_bit, t_byte, baud));
subplot(2,1,2); hold on;
hex = {'49','3A','32','30','34','38','2C','51','3A','32','30','35','31','0D','0A'};
for k = 1:n_lin
    bloque((k-1)*t_byte, k*t_byte, 1, claro(morado), txt15{k});
    text((k-0.5)*t_byte, 0.55, hex{k}, 'HorizontalAlignment', 'center', 'FontSize', 10);
end
cota(0, n_lin*t_byte, 1.45, sprintf('15 bytes = %.1f µs', t_lin));
xlim([-10 n_lin*t_byte+10]); ylim([0.3 1.8]);
set(gca, 'YTick', []);
grid on; box on;
xlabel('tiempo (µs)');
title('Figura 4b. Línea "I:2048,Q:2051\r\n" (abajo, cada byte en hexadecimal)');
guardar(f, carpeta, 'fig4_byte_linea');

%% Figura 5: una linea por par no cabe; una cada 2 pares si
f = nueva_figura([100 100 1000 400]);
ax = gca; hold on;
filas = {'1 línea cada 2 pares', '1 línea por par', 'Timer0'};
tfin = 4500;
senal_digital([0 0; 0 1; 1 0; 1000 1; 1001 0; 2000 1; 2001 0; 3000 1; 3001 0; 4000 1; 4001 0], 3, tfin);
% una linea por par: la cola nunca se vacia y cada linea sale mas tarde
t0 = tx_ini;
for k = 0:2
    bloque(t0, t0+t_lin, 2, claro(rojo), sprintf('línea %d', k+1));
    t0 = t0 + t_lin;
end
text(tx_ini+3*t_lin-5, 2.45, sprintf('atraso +%.0f µs por línea', t_lin-t_Ts), 'HorizontalAlignment', 'right', 'Color', rojo, 'FontSize', 11);
% una linea cada 2 pares: cabe
for p = [0 2000]
    bloque(p+tx_ini, p+tx_ini+t_lin, 1, claro(morado), '');
end
bloque(4000+tx_ini, tfin, 1, claro(morado), '');
text(tx_ini+t_lin/2, 1, sprintf('%.0f µs', t_lin), 'HorizontalAlignment', 'center', 'FontSize', 11);
text(2000+tx_ini+t_lin/2, 1, sprintf('%.0f %% de la línea', 100*t_lin/T_lin), 'HorizontalAlignment', 'center', 'FontSize', 11);
for p = [1000 2000 3000 4000]
    plot([p p], [0.5 3.5], 'k:');
end
terminar(ax, filas, [-10 tfin], [0.4 3.6], ...
    sprintf('Figura 5. Una línea por par ocupa el %.0f %% (no cabe); una cada 2 pares, el %.0f %%', 100*t_lin/t_Ts, 100*t_lin/T_lin));
guardar(f, carpeta, 'fig5_ocupacion_linea');

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
