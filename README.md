ZIEL:
Ausgehen von StanceCode wollte ich für meine YOSE Vorderrad Umbausatz 
eine rechtlich sichere 25km/h Grenze sowie Handgas auf 6km/h begrenzt implementieren.
GGF. noch eine besser PAS Stromstärkenaufteilung der einzelnen Stufen

Dazu diente der Quellcode von StanceCode und Anderen als Vorlage

Vorgehensweise:

Mit Opdencode dann Chatgpt und zum Schluss wieder OpenCode konnte ich den Quellcode 
dahingehend ändern , dass dies meien Ansprüchen genügt.

Vorweg CHATGPT ist absoluter SCHROTT bei der Unterstützung. 
Ich habe hier 2-3 Tage mit vergeudet "bessere" "absolute" .. Codeschnipsel zu bekommen
Wo Chatgpt selber nicht mehr wusste was es macht und ich auchnicht mehr
z.b. Zuer ermittlöung der Richtigen Reihenfolge der Ansteuerung der Kommutation sollte ich 
im Debugger der STM32CubeIde das ganze mühselig analysieren. 
Erst auf meinen eigenen Vorschlag mir sinnvolle Beispiele zugeben und damot einfach zu testen
War das rumgestochere von CHATGPT an der Stelle beendet 5 Versuche und ich hatte die richtige Reihenfolge bei der sich das Rad kraftvoll ohne brummen drehte.
Ähnliches wieder holete sich bei der Kommunikation zwischenController und C500. Chatgpt ist hier dermaßen abgedriftet, dass ich alles verworfen habe
und zu OPENCODE wiedergegangen bin,
Innerhalb von 30 Minuten war das Thema gegessen

Resultat bzw notwendige Änderungen

1.) Die Reihenfolge der Ansteuerung der Kommutation musste an mein Vorderrad (Baujahr 2021/22) angepasst werden
2.) Die Geschwindigkeitsbegrenzung fest eingebaut (Achtung im C500 kann man > 25 einstellen. Es wird aber fest auf 25 begrenzt!! Dito Handgas)
3.) Kommunikation mit C500
    -> #define DEBUG  Modus im Quellcode führt zu Baudrate 56000 womit C500 nicht kann (9600 Baudrate ) -> entfernt
    Außer dem kommuniziert der Debugoutput mit dem gleichen UART wie der C500, was Müll erzeugt

    Die gesendeten und empfangenen Bytes wurde falsch ausgewertet (E09)
    OPENCODE (Modell-ID: big-pickle ) lieferte eine korrigierte Fassung
    

Noch etwas zum Controller:
Auf disem befinden sich 4 Anshlüsse für die Programmierung perST-Link
In die habe ich Pfosterstecker gelötet ud mit dem ST-Link verbunde.#
OHNE STROM AM Controller muss der 3,3 Volt anschluss verbunden sein damit der Chip programmiertr werden kann
MIT STROM AM Controller DARF der 3,3 Volt anschluss NICHT verbunden sein.
Das war eigentlich der Normalfall während der entwicklung. Bei eingeschaltetem C500 kann damit 
mit dem STM32CUBEProgrammer das Programm geladen und auch mit der STM32CubdeIDE gedebugged werden.

Der Chip ist normalerweise schreibgeschütz. Er sollte  wenn man Opitonbytes passend setzt wieder lesbar/schreibbar sein.
Das hat bei mir nicht geklappt. Auch wenn man die Pins für reset oder bootloader kurzzeit aktiviert hätte es gehen sollen.
Beim Versuch die Pins mit Litzen zuversehen habe ich dann den Chip bzw. die Beinchen etwas verbogen.
Mit KaptabonBand und Heislsut den Chip ausgelötet und einen Neuen rein. Dann gings mit programmieren. 
Das Heisluftlöten war aber in dem engen Raum auf dem Kontroller nicht so einfach. Mit Lötkolben die Pins nochmals nachgelötet
Mit viel viel Flussmittel










#------------------------- ALT -------------------------------------------------------
# Lishui9FET
see manual in release area

Be careful, only use the "new" C500 display from Yose, the old C500 may kill the controller hardware!!!
