# 125A CYBERATOR – PC-Echtzeitdiagnose (P032)

1. ZIP vollständig in einen beschreibbaren Ordner entpacken. Nicht direkt aus dem ZIP starten.
2. Andere Audioanwendungen schließen, normale Energieeinstellung beibehalten, **start.bat** doppelklicken.
3. Das erzeugte **CYBERATOR-report-*.zip** an ChatGPT übergeben. Keine Installation oder Administratorrechte nötig. Falls der Windows-C++-Laufzeitbibliothek fehlt, meldet Windows einen Startfehler; nichts automatisch nachinstallieren.

Standard: 44,1 und 48 kHz, 64 und 256 Samples, jeweils neutral und voll aktiver Effekt; eine und vier seriell berechnete Instanzen sowie vier unabhängige parallele Instanzen. Pro Fall 2048 Callback-Gruppen nach 4 Sekunden Warmup. Optional in der Eingabeaufforderung `start.bat full`: zusätzlich 96 und 192 kHz. `smoke` ist der kurze CI-Vergleich (48/96/192 kHz, 512 Gruppen).

Exit 0 bedeutet keine verpassten Fristen in der geprüften Matrix. Exit 3 bedeutet gemessene Fristüberschreitungen; der Report ist trotzdem vollständig. Andere Fehler bedeuten unvollständige Ausführung. Alle Messwerte bleiben erhalten. Manifest und SHA256-Prüfung binden das Paket an exakt das bereits unabhängig geprüfte Entwicklungsplugin P032; keine Veröffentlichung oder Installation in den VST3-Systemordner.

**Wichtig:** Der Test ruft die echte VST3-Audioverarbeitung in einem getakteten Testhost auf. Es ist kein ASIO-Treiber- oder Audiointerface-Test und kein DAW-Praxistest. Gemessene Callback-Dauer enthält auch Thread-Unterbrechungen durch Windows. Die tatsächliche ASIO-Abnahme in Studio Pro bleibt separat. Keine Aussage über echte Dropouts allein aus den synthetischen Deadlines. Der Bericht enthält CPU-Modell, Betriebssystemversion, Energieplan und Messwerte, aber erfasst keine Seriennummern, Rechnernamen oder Netzwerkinformationen. Der Test lädt nichts aus dem Internet. Keine Systemänderungen.
