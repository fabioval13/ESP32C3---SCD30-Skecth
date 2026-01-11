
Lo sketch realizza un monitor di qualità dell’aria basato su sensore Sensirion SCD30, con:
- misura di CO₂, temperatura e umidità
- display OLED
- web server integrato per monitoraggio e configurazione
- sincronizzazione oraria via NTP
- salvataggio configurazioni in flash (Preferences)
È pensato per ESP32 / ESP8266 (con codice condizionale), in particolare ESP32-C3 / ESP32-S3.

🔌 Hardware utilizzato
Microcontrollore: ESP32 / ESP8266
Sensore: SCD30 (CO₂ NDIR + T + RH, I²C)
Display: OLED SSD1306 128×64 (I²C)
LED: LED onboard
Wi-Fi: integrato
Clock: sincronizzato via NTP

🧠 Architettura logica

1️⃣ Gestione Wi-Fi e Web Server
Si connette alla rete Wi-Fi:

ssid = "___"
password = "__"

Avvia un web server HTTP sulla porta 80
Espone due pagine web:
/ → monitor valori
/config → configurazione sensore

2️⃣ Lettura sensore SCD30
Il sensore viene letto ogni 2 secondi
Valori acquisiti:
CO₂ (ppm)
Temperatura (°C)
Umidità (%)
I valori vengono separati in:
valori live (aggiornati spesso)
valori pubblici (mostrati su display/web ogni 30 s)

👉 Questo evita continui refresh di display e web.

3️⃣ Gestione display OLED
Aggiornato ogni 30 secondi
Mostra:
indirizzo IP
valore CO₂ in grande
temperatura e umidità
Usa librerie Adafruit GFX + SSD1306

4️⃣ Web interface (monitor)
La pagina principale (/) mostra:
CO₂ in ppm (evidenziata)
Temperatura
Umidità
Ora ultima lettura (NTP)
Link alla pagina di configurazione

🔄 La pagina si auto-aggiorna ogni 30 s

5️⃣ Web interface (configurazione)

Pagina /config con:
Offset temperatura (°C)
Abilitazione/disabilitazione ASC
ASC = Automatic Self Calibration CO₂
Al salvataggio:
i valori vengono applicati in tempo reale
salvati in flash NVS (Preferences)

6️⃣ Persistenza dati (Preferences)
Usa lo storage NVS per:
offset temperatura (t_offset)
stato ASC (asc)

👉 Le impostazioni restano dopo reboot o power-off

7️⃣ Sincronizzazione oraria NTP
Usa UDP + pool.ntp.org
Recupera ora UTC
Applica offset UTC+1
Mostra l’ora dell’ultima misura
Se NTP fallisce → mantiene l’ultima ora valida

8️⃣ Gestione temporizzazioni (NON bloccante)
Usa millis() per:
lettura sensore → ogni 2 s
aggiornamento display/web → ogni 30 s

🔁 Flusso del loop principale
Gestione richieste HTTP
Lettura sensore ogni 2 s
Ogni 30 s:
copia valori live → pubblici
sincronizza ora NTP
stampa su seriale
aggiorna display OLED
🖨 Output seriale
Ogni 30 s stampa:
CO2=xxx ppm  T=yy.y C  H=zz.z
Ora NTP: hh:mm:ss

✅ In sintesi

Questo sketch è un sistema completo di monitoraggio CO₂ con:
sensore professionale (SCD30)
UI web + OLED
configurazione persistente

gestione tempo reale pulita
