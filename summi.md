2a)
(i) 
k*LOAD (k)
CLONE  (2*a)   mit a=k
EMPTY  (4*a+b) mit a=0, b=k
k + 2*k + k = 4k € theta(k)

(ii)
k*LOAD   (k)
CLONE    (2*a)   mit a=k
2*k*LOAD (2*k)
CLONE    (2*a)   mit a=2*k
EMPTY    (4*a+b) mit a=0, b=3*k
k + 2k + 2k + 2*2k + 3k = 12k € theta(k)

2b)
Das Konto wird aus der Perspektive eines jeden LOADS gesehen.
Jeder LOAD zahlt 1 Energie zum laden des Bibers plus armortisiert 4 weitere Enrgieeinheiten auf das Konto.
Folgt darauf ein EMPTY, so kann der LOAD mit den 4 weiteren Energieeinheiten für das Entfernen
seines Orginalbibers bezahlen. Subsequuente EMPTY Operationen sind für diesen LOAD kostenlos da
der assoziierte Orginalbiber bereits entfernt ist. Kommt vor dem EMPTY ein CLONE, 
so werden 2 aufgesparte Energieeinheiten für das CLONE ausgegeben. Subsequente CLONE befehle 
kosten für diesen LOAD nichts mehr da der Orginalbiber schon gecloned wurde. Folgt nun ein EMPTY, so
kostet es für diesen LOAD die letzen beiden Energieeinheiten zum Entladen der beiden geclonten Biber.
Subsequente EMPTY Befehle kosten wiederum für diesen LOAD nichts. 

2c)

(17) Für die Maschine M bezeichen wir mit Mvor den Zustand vor und mit Mnach den Zustand nach einer Operation.
(16) Wir nutzen die Potentialfunktion Φ(M) = 4 · α + β. Da die Anzahl der originalen bzw. geklonten Biber in der Maschine nicht-negativ ist, ist auch die Potentialfunktion gültig.
(5) Damit zeigen wir, dass die amortisierten Kosten jeder Operation konstant sind, indem wir die tatsächlichen Kosten einer Operation zusammen mit ihrer Potentialänderung betrachten.

(14) Load:
(6) Wir laden einen originalen Biber ein und haben damit eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = (4 · (α + 1) + β) − (4 · α + β) = 4.
(3) Somit liegen die amortisierten Kosten bei 1 + 4 = 5 ∈ Θ(1).

(21) Clone:
(19) Durch diese Operation ersetzen wir die α originalen Biber durch 2·α geklonte Biber.
(18) Dadurch haben wir eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = (0 + (2 · α + β))− (4 · α + β) = −2 · α.
(10) Somit liegen die amortisierten Kosten bei 2 · α − 2 · α = 0 ∈ Θ(1).

(15) Empty:
(9) Durch diese Operation werden alle α originalen Biber und alle β geklonten Biber aus der Maschine entfernt.
(20) Dadurch haben wir eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = 0 − (4 · α + β) = −(4 · α + β).
(13) Somit liegen die amortisierten Kosten bei 4 · α + β − (4 · α + β) = 0 ∈ Θ(1).

Entfernt:
    (11) Clean:
    (4) Somit liegen die amortisierten Kosten bei 4 · α + β + α − β = 5 · α ∈ Θ(α).
    (8) Somit liegen die amortisierten Kosten bei 2 · α − 2 · α = 0 ∈ Θ(1).
    (12) Wir laden einen originalen Biber ein und haben damit eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = 1.
    (1) Dadurch haben wir eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = 0 − 2 · α = −2 · α.
    (2) Wir nutzen die Potentialfunktion Φ(M) = β − α. Da die Anzahl der originalen bzw. geklonten Biber in der Maschine nicht-negativ ist, ist auch die Potentialfunktion gültig.
    (7) Wir nutzen die Potentialfunktion Φ(M) = 2 · α. Da die Anzahl der originalen Biber in der Maschine nicht-negativ ist, ist auch die Potentialfunktion gültig.
    (22) Dadurch haben wir eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = 0 − (β − α) = α − β

2d)
Φ(M) = 5 · α + β

Für die Maschine M bezeichen wir mit Mvor den Zustand vor und mit Mnach den Zustand nach einer Operation.
Wir nutzen die Potentialfunktion Φ(M) = 4 · α + β. Da die Anzahl der originalen bzw. geklonten Biber in der Maschine nicht-negativ ist, ist auch die Potentialfunktion gültig.
Damit zeigen wir, dass die amortisierten Kosten jeder Operation konstant sind, indem wir die tatsächlichen Kosten einer Operation zusammen mit ihrer Potentialänderung betrachten.

Load:
Wir laden einen originalen Biber ein und haben damit eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = (5 · (α + 1) + β) − (5 · α + β) = 5.
Somit liegen die amortisierten Kosten bei 1 + 5 = 6 ∈ Θ(1).

Clone:
Durch diese Operation ersetzen wir die α originalen Biber durch 2·α geklonte Biber.
Dadurch haben wir eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = (0 + (3 · α + β)) − (5 · α + β) = -2 · α.
Somit liegen die amortisierten Kosten bei 2 · α − 2 · α = 0 ∈ Θ(1).

Empty:
Durch diese Operation werden alle α originalen Biber und alle β geklonten Biber aus der Maschine entfernt.
Dadurch haben wir eine Potentialänderung von Φ(Mnach) − Φ(Mvor) = (5 · 1/5 · α + 0) − (5 · α + β) = −(4 · α + β).
Somit liegen die amortisierten Kosten bei 4 · α + β − (4 · α + β) = 0 ∈ Θ(1).

3a)
i = 2, j = 6, sum = 7, length = 4

3b)
Als erstes initialisieren wir eine Variabe welche die Gesamtsumme speichert mit 0.
Wir gehen in einer Schleife über alle Werte der Liste und addieren bei jedem Schritt
den Lsiteneintrag zu der Gesamtsumme und speichern das Zwischenergebnis in der Ausgabeliste
der Präfixsummen.

3c)
Die Summe des Intervals i, j ist die Präfixsumme bis zu j minus die Präfixsumme bis zu i.

3d)
Zuerst estellen wir eine Variable die das aktuell größte 
Intervall sowie die Intervallgrenzen speichet und initialisieren 
diese mit i=0 und j=0.
Nun gehen wir in einer Schleife durch alle Werte von i [0, n-1] und
in einer inneren Schleife durch alle Werte von j [i, n].
Finden wir dabei ein größeres Intervall j-i so prüfen wir ob die Intervallsummen
der beiden Listen übereinstimmen. Falls ja ersetzen wir das aktuell gröste gefundene 
Intervall in der Variable sowie die dazugehörigen Intervalggrenzen mit den neu gefundenen.

Existiert kein Intervall bei dem die Summen übereinstimmen, so befindet sich nach durchlaufen
der Schleifen immernoch i=0 und j=0 im Speicher, was eine leere Summe und somit 0 ist.

4a)
I = sum_a(0, i) - sum_b(0, i)
J = sum_a(0, j) - sum_b(0, j)
wenn sum_a(i, j) = sum_b(i, j):
    I = J
da wenn die Summen gleich sind die Differenz in J von dem Subinterval (0, i) kommen muss
und dies ja dasselbe Intervall ist wie I

wenn sum_a(i, j) != sum_b(i, j):
    I != J

4b)
