ZIEL: Ausgehend von StanceCoke wollte ich für meinen YOSE-Vorderrad-Umbausatz eine rechtlich sichere 25-km/h-Grenze sowie ein auf 6 km/h begrenztes Handgas implementieren.
Ggf. noch eine bessere PAS-Stromstärkenaufteilung der einzelnen Stufen.

Dazu diente der Quellcode von StanceCoke und anderen als Vorlage.

Vorgehensweise:

Mit OpenCode, dann ChatGPT und zum Schluss wieder OpenCode konnte ich den Quellcode dahingehend ändern, dass er meinen Ansprüchen genügt.

Vorweg: ChatGPT ist absoluter SCHROTT bei der Unterstützung. 
Ich habe hier 2–3 Tage damit vergeudet, „bessere“, „absolute“ … Codeschnipsel zu bekommen, wo ChatGPT selber nicht mehr wusste, was es macht, und ich auch nicht mehr.
Z. B. zur Ermittlung der richtigen Reihenfolge der Ansteuerung der Kommutierung sollte ich im Debugger der STM32CubeIDE das Ganze mühselig analysieren. 
Erst auf meinen eigenen Vorschlag, mir sinnvolle Beispiele zu geben und damit einfach zu testen, war das Herumgestochere von ChatGPT an der Stelle beendet 
Nach 5 Versuchen hatte ich die richtige Reihenfolge, bei der sich das Rad kraftvoll ohne Brummen drehte. 
Ähnliches wiederholte sich bei der Kommunikation zwischen Controller und C500. 
ChatGPT ist hier dermaßen abgedriftet, dass ich alles verworfen habe und zu OpenCode zurückgegangen bin. 
Innerhalb von 30 Minuten war das Thema gegessen.

Resultat bzw. notwendige Änderungen:

1.) Die Reihenfolge der Ansteuerung der Kommutierung musste an mein Vorderrad (Baujahr 2021/22) angepasst werden. 
2.) Die Geschwindigkeitsbegrenzung fest eingebaut (Achtung: Im C500 kann man > 25 einstellen. Es wird aber fest auf 25 begrenzt! Dito Handgas). 
3.) Kommunikation mit C500 -> Der #define-DEBUG-Modus im Quellcode führt zu einer Baudrate von 56000, womit das C500 nicht kann (9600 Baud) -> entfernt. 
Außerdem kommuniziert der Debug-Output über denselben UART wie das C500, was Müll erzeugt.

Die gesendeten und empfangenen Bytes wurden falsch ausgewertet (E09).
OpenCode (Modell-ID: big-pickle) lieferte eine korrigierte Fassung.


Noch etwas zum Controller: Auf diesem befinden sich 4 Anschlüsse für die Programmierung per ST-Link. In die habe ich Pfostenstecker gelötet und mit dem ST-Link verbunden. 
OHNE STROM AM CONTROLLER muss der 3,3-Volt-Anschluss verbunden sein, damit der Chip programmiert werden kann. 
MIT STROM AM CONTROLLER DARF der 3,3-Volt-Anschluss NICHT verbunden sein. Das war eigentlich der Normalfall während der Entwicklung. 
Bei eingeschaltetem C500 kann damit mit dem STM32CubeProgrammer das Programm geladen und auch mit der STM32CubeIDE gedebuggt werden.

Der Chip ist normalerweise schreibgeschützt. Er sollte, wenn man die Optionsbytes passend setzt, wieder lesbar/schreibbar sein. 
Das hat bei mir nicht geklappt. Auch wenn man die Pins für Reset oder Bootloader kurzzeitig aktiviert, hätte es gehen sollen. 
Beim Versuch, die Pins mit Litzen zu versehen, habe ich dann den Chip bzw. die Beinchen etwas verbogen. 
Mit Kaptonband und Heißluft den Chip ausgelötet und einen neuen eingelötet. Dann ging es mit dem Programmieren. 
Das Heißluftlöten war aber in dem engen Raum auf dem Controller nicht so einfach. 
Mit dem Lötkolben die Pins nochmals nachgelötet. Mit viel, viel Flussmittel.

#------------------------- ALT -------------------------------------------------------

Lishui9FET
See the manual in the release area.

Be careful: only use the "new" C500 display from Yose. The old C500 may kill the controller hardware!
