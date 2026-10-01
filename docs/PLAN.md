# Totality: Plan für den Hypnotic-Berlin-Techno-Generator

Stand 27.09.2026, Entwurf nach den Entscheidungen des Nutzers (Abschnitt 16). Name **Totality** (bis 28.09.2026
**Umbra**, Risiko 7), Repo `G:\Tools\VRAudio\TechnoGenerator` (eigenes Projekt, nicht in Noctuary integriert),
Namensraum `tot::`, Präfix `TOT_`, Werkzeuge `tot_render`, `tot_selftest`, Set-Datei `.totset`. Code-Kommentare im
Doxygen-Format wie Phosphene und Ephemeris; GUI und Handbuch auf Englisch, Plan und Journal auf Deutsch.

Der Name: Die Totalität ist der Abschnitt einer totalen Sonnenfinsternis, in dem der Mond die Sonne ganz verdeckt;
übrig bleiben die schwarze Scheibe und ihre Korona. Was dort bleibt, ist Form ohne Glanz. Berliner Techno nimmt weg,
was glänzt (Melodie, Akkordwechsel, Drops), bis Puls, Raum und kleine Veränderung übrig bleiben, und in diesem Rest
geschieht die Hypnose. Die Geschwister heißen Noctuary, Phosphene und Ephemeris; Totality gehört in dieselbe Familie
der Himmels- und Lichtwörter. Bis zum 28.09.2026 hieß das Projekt Umbra, nach dem Kernschatten einer Finsternis; der
Name ist als Audio-Software vergeben (Risiko 7).

**Grundlagen dieses Entwurfs.** Das Recherchedokument des Nutzers (`docs/research/Berlin-Techno-Analyse-2026-09-27.pdf`,
im Folgenden **Dok.** mit Abschnittsnummer), seine Ergänzung (`docs/research/Ergaenzung-2026-09-27.md`, **Erg. 1** bis
**Erg. 8**, geprüft in 2.11), der Code und die Pläne von Noctuary (`G:\Tools\VRAudio\AmbientSynth`), Phosphene
(`G:\Tools\VRAudio\PsytranceGenerator`) und Ephemeris (`G:\Tools\VRAudio\BerlinSchoolGenerator`), drei gezielte
Websuchen zu Behauptungen der Ergänzung (Quellen am Ende). Zahlen tragen die Kennzeichnung des Dokuments: **[Q]** direkt
aus einer zitierten Quelle, **[A]** daraus abgeleitet, **[I]** inferiert und zu kalibrieren. Was hier neu vorgeschlagen
wird, ist [I], bis es gemessen ist.

## Stand der Umsetzung

**01.10.2026: Phase 21, Track oder Mix, der Zoom und der Anfang eines Mixes.** Der Nutzer: "In der GUI ist es etwas
verwirrend, ob man jetzt einen Einzeltrack erzeugt oder einen Mix"; bei langen Mixes und Tracks ein Zoom "etwa mit dem
Mausrad"; und sein Mix "begann ... wieder mit einer Solo-Kick".

- **Track oder DJ-Mix.** Statt "Track min" und "Set min" (0 hieß: ein Track) zwei Knöpfe "Track | DJ mix" neben der
  Tonart und ein Längenregler für das Gewählte (Track 2 bis 16 min, Mix 10 min bis 12 h, die ersten zwei Stunden über
  den größten Teil des Wegs); ein Klick komponiert sofort, Compose heißt "Compose track" oder "Compose mix" und leuchtet,
  solange das Gespielte das andere ist; die Statuszeile beginnt mit "single track" oder "DJ mix of N tracks". Die Länge
  des Mixes bleibt gemerkt, solange ein Track gewählt ist (`TotalityProcessor::chooseMix`, im State `mixMinutes`).
  `set.minutes` bleibt der Parameter (0: ein Track): Host, `.totset`, Quest und `tot_render` unverändert.
- **Zoom** (`ArrangeView`, im Streifen oben und auf der Arrange-Seite): das Mausrad zoomt um den Zeiger bis auf vier
  Takte, Ziehen, Shift + Rad oder ein seitliches Rad verschieben, ein Doppelklick zeigt alles; ein Lineal (die Takte
  eines Tracks, auf der Arrange-Seite mit der Zeit, die Minuten eines Mixes); die Matrix eine Spalte je Pixel, ihre
  Helligkeit die Deckung je Schlag gegen die typische der Spur, so dass sie in jedem Zoom gleich liest und gezoomt jeder
  Schlag einzeln steht; ein gezoomtes Bild blättert mit dem Abspielkopf weiter, wenn er hinausläuft (nicht, wenn man es
  von ihm weggeschoben hat); der Streifen oben markiert das Fenster der Arrange-Seite. `TOT_SHOT_ZOOM` ("von:bis" in
  Schlägen) für die Screenshots.
- **Der Anfang eines Mixes, gemessen.** 24 Mixe (9 min, Seeds 101-124; `intro_ref.py` und die Stems): 5 begannen als die
  Kick allein oder mit gedämpften Hats. Die Ursachen: der erste Track eröffnete in 9 von 24 mit *drums* (die Kick ab
  Takt 1); die eröffnende Percussion war oft ein Tom oder Rim aus der Matrix mit ein bis zwei Schlägen je Takt, am
  unteren Rand ihres Fensters (Seed 110: der Tom 24 dB unter der Kick, die Percussion des Bodys 13); die Hats lagen in
  Takt 1-2 hinter dem 4-kHz-Filter noch 34 dB darunter.
- **Drei Änderungen.** Ein Tom oder Rim, der den Track eröffnet, bekommt eine euklidische Figur auf den zwölf
  Sechzehnteln neben den Vierteln (E(5,12), E(6,12) oder E(7,12), gedreht; aus dem Strom des Racks), wenn der Rack ihm
  keinen Zyklus und keine Figur gab -- vier bis sieben Schläge je Takt; seine Spur steht oben in ihrem Balance-Fenster
  (die oberen 2 dB statt des ganzen, `corrections`); der erste Track eines Mixes eröffnet nie mit der Kick (*percussion
  first* oder *atmosphere first*, die Kick auf Takt 5 oder 9).
- **Ergebnis.** Mixe "Kick allein" oder "Kick + Hats" 5 -> 1 von 24 (die eine: ein Roller mit tiefem Tom, den das Maß
  nicht hört; in den Stems liegt er bei -20 dB, und die Kick kommt erst mit Takt 5), keine Kick vor Takt 5; Seed 110:
  Takt 1-8 der Tom bei -18 statt -24 dB, die Hats in Takt 1-2 bei -27 statt -34 dB, die Kick ab Takt 9. Einzelne Tracks (vier Stile,
  sechs Seeds) 3 -> 1 von 24 (ein Hypnotic Tribal mit dunkler Conga unter der Kick ab Takt 1). `testComposer`: ein
  eröffnender Tom oder Rim mit drei Schlägen je Takt und mehr, der erste Track eines Mixes ohne Kick bis Takt 5 (24
  Tracks); 187/187.
- **Nebenbei gefunden und am selben Tag behoben:** der Rack zog für Tom, Rim und Shaker mit p 0,4 E(3,8) doppelt,
  E(5,16) oder E(7,16) "neben den Vierteln" -- die beiden letzten haben einen Schlag auf jedem Platz des Viertels, keine
  Drehung hält sie davon frei, und die Spur fiel still auf die Matrix zurück (der Rim mit .4/.2/.2: unter einem Schlag
  je Takt). Jetzt liegen fünf oder sieben Schläge als E(5,12) und E(7,12) auf den zwölf Sechzehnteln neben den Vierteln
  (`offQuarterMask`, Rack.h; dieselbe Zahl Ziehungen, so dass alles danach Gezogene bleibt). In 200 Plänen 143
  euklidische Spuren statt 37 (55 mit fünf, 37 mit sechs, 51 mit sieben Schlägen; `testRackPhase2` prüft alle drei).
  Die Taktähnlichkeit bleibt (48 Tracks, je Stil ±0,002), je Takt 1 bis 2 Anschläge mehr. Der Test "rerolling the
  offbeat hat ... keeps the untied layers" verglich auch die Velocity des Pings, der aber dort nachgibt, wo eine Hat ihn
  trifft (`LayerDef::perc`): er vergleicht beim Ping jetzt Zeiten und Tonhöhen. Dabei auch behoben: die zwei Ziehungen
  der Eröffnungsfigur standen als Argumente eines Aufrufs (ihre Reihenfolge wäre die des Compilers: MSVC, icx und der
  Quest-Clang hätten verschiedene Figuren ziehen können).
- `tot_render`: `--seconds S` rendert nur den Anfang; die Liste eines Sets nennt je Track die Eröffnung, den Einsatz der
  Kick und ihren Abgang.

**30.09.2026: Phase 20, Anfang und Ende wie auf den Platten.** Der Nutzer: "Ich bin mir sehr sicher, dass nahezu keiner
mit einer simplen 4-to-the-floor Kick alleine beginnt" -- und dasselbe für das Outro. Bisher: Takt 1 bis 8 die Kick
allein, dann Closed Hat (9), halbe Rolling Hat (17), eine Percussion (25); im Outro fiel alle 8 Takte etwas weg, die Kick
blieb bis zum Schluss.

- **Gemessen an den 30 Referenzplatten** (neu: `intro_ref.py`/`outro_ref.py` im Scratchpad der Sitzung; je 2 Takte der
  ersten bzw. letzten 64, Kick als Puls auf den Vierteln im 40-150-Hz-Band, Hats 7-10,5 kHz, Percussion perkussiv
  400-4000 Hz abseits der Kick-Schläge, tonal harmonisch 200-4000 Hz, jedes gegen seinen Pegel im Body). **Anfang**
  (Takt 1-8): nur 2 mit der Kick allein, 1 mit Kick und Hats; 20 haben Percussion ab den ersten Takten, 18 halten die
  Kick 8 bis 24 Takte zurück (oder spielen sie gefiltert) unter Percussion, Hats oder einer Fläche. Nach Stil: Hypnotic
  2-3 von 7 mit der Kick sofort, Ostgut 3 von 11, Dub 1 von 6, Raw 4-5 von 6. **Ende** (letzte 8 Takte): 22 von 30 ohne
  Kick -- sie geht 4 bis 36 Takte vor dem Ende, 8 haben sie in den letzten 64 Takten gar nicht mehr; Hypnotic nie in den
  letzten 8 Takten, Raw 4 von 6 bis zum Schluss. Die Hats sind in 18 von 30 vorher gegangen (Median 9 Takte vor dem
  Ende); Percussion und ein tonales Bett tragen die letzten Takte, danach 1 bis 21 s Ausklang.
- **Die Literatur** sagt dasselbe für den Anfang (TrackSensei: "drums and percussion only: kick, hats, maybe a filtered
  loop"; Dok. 8.5: Kick, Closed Hat, eine Perc) und für das Ende uneinheitlich: "mirror the intro", "subtracts
  elements every 8 bars until only the kick remains" oder "until only percussion or a fading texture remains", ein
  sauberer Schnitt auf dem Downbeat (Mixed In Key, Mastrng, EDM Ghost Production). Die Platten entscheiden für die
  Percussion und die Fläche.
- **Drei Eröffnungen** (`Opening`, `Composer.h`), je Stil gewichtet: *drums* (Kick, Closed Hat und die Intro-Perc ab
  Takt 1, die Rolling Hat auf der ersten 8-Takt-Linie), *percussion first* (Closed Hat, Rolling Hat und die Perc ab
  Takt 1, die Kick auf Takt 9 oder 17), *atmosphere first* (Textur und Perc ab Takt 1, die Hats auf den 8- und
  16-Takt-Linien, die Kick auf 9 oder 17); Gewichte Hypnotic 0,35/0,30/0,35, Ostgut 0,30/0,30/0,40, Dub 0,20/0,20/0,60,
  Raw 0,70/0,15/0,15. Das Hat-Filter öffnet ab 4 kHz statt 1,5 kHz (von dort lagen die Hats 30-40 dB unter der Kick,
  und ein Drums-Anfang klang wie die Kick allein); die Spur, die den Track eröffnet, behält in der Balance ihr Fenster
  (`LevelMark::front`).
- **Das Ende**: die Kick geht 0, 8, 16 oder 32 Takte vor dem Ende (je Stil gewichtet, mindestens so weit, wie die
  Eröffnung sie zurückhielt), die Closed Hat 8 oder 16 Takte vorher oder gar nicht (bleibt: Hypnotic 20 %, Ostgut 45 %,
  Dub 25 %, Raw 35 %); Intro-Perc, Textur und, wo die Hat geht, eine zweite Percussion bleiben bis zum Schluss. Die
  Textur darf einen Track beenden (keine Tonart, keine Reibung im Mix).
- **Ergebnis, mit denselben Maßen an 24 eigenen Tracks:** Anfang "Kick allein" oder "Kick + Hats" 3 von 24 (Platten 3
  von 30; unsere drei sind Roller/Tribal mit einer spärlichen Tom als Intro-Perc), Kick zurückgehalten 16 von 24 (18
  von 30); Ende ohne Kick 20 von 24 (22 von 30), die Kick geht im Median 18 Takte vorher (Platten etwa 20). Die
  Hörbarkeit im Body bleibt (Percussion −10,9 dB gegen die Kick, in 91 % der Fenster). `testComposer`: nie die Kick
  allein, zwei Stimmen neben ihr in den ersten und letzten vier Takten, die Kick bis Takt 17, alle drei Eröffnungen;
  187/187.
- **Offen:** Hören. Der Ausklang der Platten (median gut 4 s Hall oder Fade nach dem letzten lauten Takt) ist bei uns
  kürzer (um 2 s); nicht angefasst, weil die Literatur für DJs den klaren Schnitt will.

**29.09.2026, früh: Phase 19, der Swap auf der Figur.** Der Nutzer über das Set des Phase-18-Builds: "Nach wie vor
bestehen die ersten zwei Minuten jedes Tracks im Mix nur aus Kick und Hi Hat, das ist viel zu lang."

- **Befund, am Set gemessen** (Stems des Sets, je 15 s das lauteste Sample von Hats, Percussion und tonaler Stimme gegen
  die Kick; voll = Percussion ≥ −18 und tonal ≥ −16 dB): 145 von 211 Fenstern voll, 4,2 Minuten ganz dünn -- die ersten
  zwei Minuten des Sets (der erste Track hatte als Intro-Perc einen Shaker, der auf dem Hat-Bus liegt, seinen Clap erst
  bei Takt 97) und nach jedem Swap 0,5 bis 1 Minute: Der Swap lag auf dem Bass-Einsatz des ankommenden Tracks (Takt 33),
  seine Figur kam 32 Takte später; unter ihm der abgehende Track in seinem Outro (ohne tonales Material, Mitten und
  Höhen weggezogen, Fader nach 24 Takten zu). Dazu ein Ostgut-Roller mit einem Tom-Schlag je Takt als ganzer
  Percussion für 64 Takte: Die Kappe des Pools (9) war mit Hats, Ride, Ghost-Kick und Shaker voll und hatte den Clap
  weggeschnitten, Rim und Ride waren für die zweite Hälfte zurückgehalten.
- **Der Swap auf der Figur** (`Set.cpp`, `Set.h`): der ankommende Track steht so, dass sein Takt 65 -- der zweite
  Body-Block, die Figur -- auf dem Swap liegt; Intro und erster Body-Block (2 Minuten) laufen unter den letzten zwei
  vollen Body-Blöcken des abgehenden, der Bass am Isolator gekillt und mit der Figur freigegeben. Dafür ein Block mehr
  je Track (224 statt 192 Takte, mindestens 3 Body-Blöcke), die eigene Zeit von Swap zu Swap bleibt. Dichte eines
  Set-Tracks: 0,85 im Block unter dem anderen, 1,0 ab dem Swap (der Swap ist sein Drop); der erste Track des Sets 1,0 ab
  dem Body, seine Figur auf der 8-Takt-Linie nach dem Bass.
- **Percussion, die man hört** (`Composer.cpp`): Intro-Perc ist Clap, Rim oder Tom, nicht der Shaker; Shaker und
  Ghost-Kick zählen beim Verweben der Einsätze zu den Hats und der Shaker nicht mehr zur Percussion-Mindestzahl; die
  Kappe des Pools schont Clap, Tom und Rim bis zur Mindestzahl.
- **Ergebnis:** Set 16050400 (52,8 min) 145 → 180 von 211 Fenstern voll, dünne Strecken 4,2 → 0,8 min (das
  Schluss-Outro), Percussion ab 0:15, voll ab 1:00. Einzeltracks (28): Percussion-Spitze gegen die Kick −17,1 → −13,9 dB,
  hörbar in 60 → 91 % der Body-Fenster; Stimmen je Takt in der Partitur 5,36 → 5,57. Der Wächter der Balance (Phase 18)
  zählt nur Stellen mit Kick-Band (im Set lag eine Stelle vor dem Bass-Swap und nahm alle Anhebungen zurück). `testMix`
  prüft den Swap auf der Figur; 186/186.
- **Offen:** Hören. Nach dem Swap trägt der ankommende Track 32 Takte lang zwei bis drei Percussion-Stimmen und die Figur;
  ob das dem Ohr reicht, entscheidet der Nutzer.

**28.09.2026, Nacht: Phase 18, das Gleichgewicht.** Der Nutzer über Track und Set des Builds von 19:54: "Die bestehen
zum überwiegenden Teil nur aus Kick und Hi Hats (und vielleicht ein wenig Ride)".

- **Befund, an den Stems gemessen** (das lauteste Sample jeder Stimme gegen das der Kick, Median über 28 Tracks: vier
  Stile mal sieben Archetypen): Hats -14, Percussion -21, tonale Stimme -19 dB. Die Recherche (Analyse 2026-09-27, nach
  Attack): Closed Hat -8, Open Hat -10, Conga -15, FX -10 dB. In 9 von 28 Tracks gar keine tonale Stimme: ein
  monotoner Track verbot Ping, Chord, Drone und 303, Roller, Tribal und die Hälfte der Tools hatten keine Figur. Die
  Kalibrierung zeigte das nicht: Spektrum und harmonisch-perkussive Anteile lagen im Band der Referenzen -- die Mitten
  füllten der Klick der Kick, das verzerrte Rumble und der Raum. Raws `space.level` +10 dB las in 250-1000 Hz -5,8 dB,
  lauter als jedes Instrument: die Rezepte (Phase 13.5) waren am Spektrum gefittet und hatten den Raum statt der
  Instrumente hochgezogen. Die Dichte der Partitur (Phase 10) stimmte; es fehlte, was man hört.
- **Jeder Track hat seine Figur** (`Composer.cpp`): der Roller einen Bass-Riff, wo der Sub den Bass trägt, sonst wie der
  Tribal-Track und die Tools ohne Riff die Leitstimme seines Stils (die wahrscheinlichste von Ping, Chord, 303: Hypnotic
  der Ping, Ostgut und Dub der Chord, Raw die 303); ein monotoner Track spielt sie auf Grundton und Oktave. Sie kommt mit
  dem zweiten Block des Body (die Regel "die letzten zwei Stimmen für die zweite Hälfte" hielt sie in kleinen Pools bis
  Takt 129 zurück). Hats und Percussion ziehen abwechselnd ein, jede Gruppe ihre wahrscheinlichste zuerst (Ostgut: die
  Clap im Intro statt nach Takt 137). Streuung und Jitter der ungebundenen Stimmen hängen nicht mehr am Kandidaten des
  Blocks (sonst verschob ein neu gewürfelter Hat den Ping um Millisekunden).
- **Die Balance im Leveler** (`Leveler.h`, `LevelMark::balDb`): vor der Lautheit liest er an drei Stellen (der lauteste
  Teil, 16 und 40 Takte davor; eine Stelle allein erwischte die Figur beim Ausatmen) das lauteste Sample jeder der zwölf
  Kit-Spuren und jeder tonalen Stimme gegen die lauteste Kick, und rückt, was außerhalb seines Fensters liegt, an den
  Rand (-8 bis +15 dB; im Deck an der Quelle, vor den Sends, wie ein Fader). Die Fenster nach Rolle, um die Werte der
  Recherche [I]: Closed Hat -14 bis -8, Clap und Snare -12 bis -7, Rim -14 bis -9, Tom -15 bis -10, Conga -17 bis -12,
  Shaker -18 bis -12, Leitstimmen -11 bis -8, Drone -18 bis -12, Textur -22 bis -16; Dub: Hats 6 dB tiefer,
  Percussion 3. Ein Rang je Art: zwei Stimmen vorn, die dritte 3 dB, die weiteren 6 dB tiefer; von den Leitstimmen
  eine vorn, die anderen 4 dB tiefer. Dann ein Wächter: 250-1000 Hz und 1-5 kHz gegen 40-140 Hz an denselben Stellen;
  über dem 90. Perzentil der Referenzen (dieselben Filter, Hypnotic -2,6/-4,8, Ostgut -9,9/-6,9, Dub -9,4/-18,2, Raw
  -9,2/-11,5) nimmt er erst den Raum-Return zurück (bis -12 dB), dann die Anhebungen. Plugin und Quest reichen die
  Korrekturen wie die Lautheit nach; sie gleiten ein.
- **Ergebnis (28 Tracks):** Percussion -21,4 → -12,9 dB, tonale Stimme -18,9 → -11,9 dB gegen die Kick (Median), zu
  hören in 58 bzw. 75 % der 8-Takt-Fenster des Body (vorher je 5 %); die Hats unverändert bei -13 (die
  harmonisch-perkussive Messung zeigte sie schon im Band, Hypnotics sogar darüber). Die Bänder aller zwölf Prüftracks im
  10.-90. Perzentil der Referenzen (Raw 4242 0,7 dB darüber, aus seiner Komposition). Stimmen je Takt in der Partitur
  5,27 → 5,36. `testBalance` (jede Stimme im Fenster, erneut gespielt auf 1,5 dB wie gemeint, eine Leitstimme in jedem
  Track), `testFigure` (die Figur in jedem Track bis zum zweiten Body-Block), 185/185. Komponieren und Pegeln eines
  Tracks dauert 5 bis 9 s statt 3.
- **Offen:** Hören. Die Fenster sind abgeleitet [I]; der Wächter hält das Spektrum, nicht den Geschmack.

**28.09.2026, Abend: Phasen 12 bis 17**, derselbe Auftrag ("alles, was ohne mein Feedback geht").

- **Phase 12, Dichte gegen die Referenzen** (`analyze_ref.py`, `offq_*`): das Onset-Profil eines Bandes im Mittel neben
  den Vierteln -- niedrig, wo die Kick das Band allein trägt. Es trennt, was das Hören am Set von Phase 9 fand: die
  ersten acht Minuten 0,78 (vorher) gegen 0,86 (Phase 10); die Referenzen: Hypnotic 0,91, Ostgut 0,75, Dub 0,92,
  Raw 0,81 (Median). Im Kalibrierbericht als Spalte "Mitten neben den Vierteln". Ein erster Versuch (Anteil der
  Sechzehntel über einer Schwelle) sättigte bei 0,8 bis 1,0 in Aufnahme wie Render und flog wieder heraus. Befund: bei
  24 Tracks liegen die Hälfte unter dem Band -- unsere Mitten sind trockener, die Kick trägt sie mehr als in den
  Platten. Mehr Percussion-Pegel bewegt das Maß kaum (Raw +8 dB: 0,57 → 0,59); es misst eher, wie gefüllt die Mitten
  sind (Hall, Echo, Flächen). Nicht blind eingestellt: das ist "Material" und braucht Hören (offen).
- **Phase 13, Archetypen** (`Composer.h`, `compose.archetype`): Tool, Roller, Stab, Acid, Bleep, Dub Chord, Tribal --
  eine Art des Tracks neben seinem Stil. Jede ändert Pool, Zyklen, Ereignisse und Formgewichte des Profils, bestimmt
  die Figur (Stab und Dub Chord den Akkord, Acid die 303, Bleep den Ping, der Tool zur Hälfte einen Bass-Riff, Roller und
  Tribal keine: ihre Percussion ist ihr Gesicht) und hebt ihre Stimme im Mix (Roller +3 dB Percussion, Tribal +4, Stab
  und Acid +2 auf die Figur ...). Gewichte je Stil (Hypnotic: Roller 0,35, Bleep 0,25; Ostgut: Stab und Tool je 0,25;
  Dub: Dub Chord 0,65; Raw: Acid und Tribal je 0,3). Ein Set zieht jedem Track seinen, nie den des Vorgängers
  (`testMix`); der Test der Figur fordert sie nur, wo der Archetyp eine hat.
- **Phase 14, Kalibrierung an Mixen:** Discogs verweigert den Abruf, einzelne Indexzeiten gab es nicht; Trackzahl und
  Länge schon -- Berghain 04 (Ben Klock) 19 Tracks in 70:18, Berghain 06 (Norman Nodge) 19 in 75:06, Fabric 66 (Mix-Dok.)
  24 in 73 Minuten: 3,0 bis 4,0 Minuten je Track, im Mittel 3,6. `set.track_minutes` steht jetzt auf 3,5 (vorher 3):
  etwa 17 Tracks die Stunde. Tempo-Drift und Überlagerung sind aus Tracklists nicht zu lesen.
- **Phase 15, Groove aus den Referenzen: verworfen.** Gemessen: je Schlag von seiner Kick aus die Lage der Hat- und
  Perc-Onsets auf den drei Sechzehnteln danach, parabolisch interpoliert. Der Swing streute innerhalb eines Stils von
  −16 bis +29 ms (Hypnotic), −14 bis +13 ms (Raw); ohne Quellentrennung ist das Mikrotiming aus der Mischung nicht zu
  lesen. Keine Vorlage, das Maß wieder entfernt.
- **Phase 16, Rekordbox-Export** (`writeRekordboxXml`): neben jeder WAV `<name>.wav.rekordbox.xml` -- eine TRACK mit
  Beatgrid (ein TEMPO je Takt, dessen Tempo sich änderte: die Rampen eines Sets), jeder Cue ein Memory Cue, die ersten
  acht Hot Cues, eine Playlist "Totality". Nach dem veröffentlichten Format geschrieben, in Rekordbox nicht probiert
  (`testRekordbox`).
- **Phase 17, Bewertungen** (`Preferences.h`): "+" und "−" neben der Statuszeile bewerten den Track unter dem Playhead
  (`Dokumente/Totality/ratings.tsv`: Wert, Archetyp, Stil, Seed, Preset-Gruppen). Mit "Favor Ratings" (`compose.
  use_ratings`) multiplizieren die Bewertungen die Gewichte der Archetypen und der Preset-Gruppen (1 ± 0,25 je
  Bewertung, 0,25 bis 3): acht Likes für Tribal heben seinen Anteil bei Raw von 30 auf 62 %, zwei Dislikes für Acid
  senken ihn von 30 auf 10 % (`testRatings`). Ein gespeichertes Set trägt die Bewertungen nicht: gleich nur ohne Favor.
- **Kalibrierung mit 24 Tracks** (sechs je Stil, `docs/eval/kalibrierung-phase13.md`): 34 von 192 der bisherigen Werte
  außerhalb des 10.-90. Perzentils der Referenzen (bei gleicher Verteilung wären 38 zu erwarten); Wechsel je 64 Takte
  Hypnotic 4,75 (Referenz 4,5), Ostgut 3,25 (4,6, knapp darunter), Dub 3,0 (3,7), Raw 3,3 (3,3). Nachgestellt: Hypnotics
  Hat-Welle etwas flacher (die Höhen bewegten sich 12 dB gegen 4,6), Ostgut und Dub öfter Wellen von 32 Takten, Raw
  weniger Lautheitsabsenkungen vor den Landungen. Offen: Raws Lautheitsspanne (2,7 LU gegen 1,3 -- die Raw-Platten sind
  sehr flach) und die trockenen Mitten (oben).
- **Nachträge:** Roller und Tribal eröffnen mit Tom/Conga als Intro-Perc (ihr Gesicht ist gleich zu hören); die
  zusätzlichen Einsätze des Aufbaus (Phase 10) nehmen die freien 8-Takt-Linien des Blocks (8, 16, 24), nicht nur die
  nach dem Haupteinsatz -- Stimmen je Takt neben der Kick 5,21 → 5,27 (drei Sets à 30 Minuten, acht Tracks), Takte mit
  Percussion unverändert 80 % (Sets 91-93 %). Ein langes Set (bis 720 Minuten, gut 200 Tracks) bricht im Plugin ab,
  sobald eine neuere Partitur verlangt ist (`composeSet(..., stop)`).
- **vst3test, 96 kHz stumm:** die Schleife "alle Parameter zufällig, dann Default" traf auch die 2080
  MIDI-CC-Stellvertreter des VST3-Wrappers (VST3 kennt keine CC-Ereignisse). Auf Default gesetzt senden sie CC 1 = 0 --
  das Modrad in Ruhe, und das schließt den Master-Filter ganz (Tiefpass bei 19,5 Hz). So seit Phase 5; bestanden hat
  der Test, solange genug Sub durchkam. Jetzt nimmt er die CC-Parameter aus, 37/37. Offen, zu entscheiden: das Modrad
  in Ruhe schließt den Filter -- ein Keyboard oder Host, der die Ruhelage sendet, macht das Plugin stumm (Vorschlag:
  Modrad nur Hochpass, 0 = offen, oder das Pitch-Rad mit seiner Mitte für den ganzen Filter).
- Nicht gebaut, weil es Hören oder Werkzeuge braucht, die ich nicht habe: der Ableton-Export (.als ohne Ableton nicht
  zu prüfen), Bandsättigung auf der Percussion (bräche die exakten Stems), der Test auf dem Headset.

**28.09.2026, Abend: Phase 11, Kleinigkeiten mit Wirkung.** Auf "Bitte baue alles ein, was du direkt ohne mein Feedback
machen kannst" (nach meiner Liste von Ideen):

- **Ein Seed, zwei Stile, zwei Tracks:** alle Ströme eines Tracks mischen den Namen seines Stils ein (vorher gab ein
  Seed in Hypnotic und Ostgut dieselbe Form, dieselben Landungen und Pausen an denselben Takten).
- **Das Set kommt schneller in Fahrt:** der erste Track eines Sets, unter dessen Intro nichts liegt, bringt Hats,
  rollende Hat und Perc bei Takt 5, 9 und 13 statt 9, 17 und 25 und öffnet den Hat-Bus über acht Takte
  (`TrackRequest::quickStart`).
- **Zwei Dramaturgien mehr:** Cruise (Klock, Mix-Dok. 6: "starts hard, then in 15 minutes eases into a supercruise",
  baut in der letzten halben Stunde wieder auf; 129 → 133 BPM) und Marathon (Warm-up, zwei Höhepunkte mit einem Tal,
  langes Closing; 126 → 134 bei 60 %, zurück auf 128). Ein Set darf jetzt zwölf Stunden lang sein.
- **Kein Basston außerhalb der Skala:** das Bass-Alphabet kennt b2 und b5; der Riff der Figur und die Basslinie nehmen
  sie nur, wo die Skala sie hat, sonst die Quinte (von der Stil-Mischung aufgedeckt: Dub Seed 5 und 6).

**28.09.2026, Nachmittag: Phase 10, der Groove.** Nach dem Hören von `out/p9/set30_nachher.wav`: "In den ersten 3
Minuten passiert praktisch überhaupt nichts ausser Kick und Hi-Hat ... Das kann doch so niemals der Standard im Berliner
Techno sein. Ausserdem gibt es praktisch gar keine Percussion mehr." Die Partitur gab ihm recht: der erste Track spielte
bis Takt 40 Kick, Closed Hat, rollende Hat und Shaker, ab 41 die Open Hat, erst ab 97 einen Ton; keine Clap, kein Rim,
keine Toms im ganzen Track. Drei Ursachen:

- **Der Pool schrumpfte auf die Zahl der Blöcke** ("eine Operation je Block"): ein Set-Track von fünf Blöcken behielt
  drei, vier Lagen, meist Hats; die Percussion fiel zuerst. Jetzt hält der Pool mindestens die Kappe des Stils weniger
  drei (sechs bis sieben Stimmen neben Kick und Hats), aufgefüllt aus dem, was die Ziehung ausließ -- zuerst Percussion
  (Clap, Shaker, Toms, Rim: zwei bei Hypnotic und Dub, drei bei Ostgut und Raw), dann die Wahrscheinlichsten --,
  höchstens die Kappe.
- **Die Treppe war zu langsam:** eine Lage je 32 Takte. Mix-Dok. 2 und 3: die Tool-Vorlage steht bei Takt 97 voll
  (Dichte 0,3 → 0,6 bei 33 → 0,85 bei 65 → 1,0 bei 97), der Kanon bringt auf fast jeder 8-Takt-Phrase seiner ersten
  Minuten etwas ("The Bells": 1, 9, 11, 17, 25, 33, 41, 65). Jetzt bringt ein Body-Block unter seiner Dichte nach dem
  Haupteinsatz bis zu zwei Hats oder Percussion auf den nächsten 8- und 16-Takt-Linien; bis zur Hälfte baut er bis zwei
  unter die Kappe, damit danach noch zwei Lagen kommen. Eine Loop-Stimme, deren Würfe einen Takt leer ließen, behält
  ihren wahrscheinlichsten Schlag (der Rim mit .4/.2/.2 schwieg ganze Blöcke).
- **Der Blend nahm der gehenden Platte die Percussion** (Mitten −18 dB, gegen Mix-Dok. 7: "A nur noch Hats/Perc"): jetzt
  die Mitten des Gehenden −6 dB, die des Kommenden −10 statt −18 dB.

Gemessen, dasselbe Set (Seed 2026, 30 Minuten, `out/p9/set30_dicht.wav`): Stimmen je Takt neben der Kick 4,1 → 6,1,
Takte mit Percussion jenseits der Hats 70 % → 90 %, in den ersten drei Minuten 3,1 → 3,9 Stimmen (die erste Minute bleibt
das Intro des ersten Tracks: Kick, Hats, ab Takt 17 der Rim); Lautheitsspanne 3,3 LU, Wechsel je 64 Takte 5,2. Die zwölf
Kalibriertracks (`docs/eval/kalibrierung-phase10.md`): 12 von 96 Werten außerhalb des Referenzbands wie in Phase 8, die
Taktähnlichkeit im Korridor; Hypnotic bewegt sich mit mehr Stimmen etwas weniger (3,0 Wechsel gegen 4,5, im Band).
Neuer Test `testGroove` (fünf Stimmen oder mehr im zweiten Body-Block, Percussion jenseits der Hats), `selftest`
178/178.

**28.09.2026, Mittag: Phase 9, die Mix-Engine.** Schritt 2 des Plans aus Phase 8 ("Mach ruhig schon mal mit dem Mix
weiter"): das Set als Live-Rekomposition (Mix-Dok. 6--8) statt als Staffel. Vorher lief jeder Track seinen ganzen Body
allein (4--5 Minuten, 14 Tracks die Stunde), der nächste Fader öffnete 16 oder 32 Takte vor dem Swap, Deck C spielte
selten eine Loop, zwischen den Blends rührte niemand den Mixer an.

- **Die Zeit eines Tracks** (`set.track_minutes`, 3 Minuten; Mix-Dok.: Klocks Berghain 04 "a good two or three minutes",
  Fabric 66 24 Tracks in 73 Minuten): ein Set-Track ist so lang, wie seine eigene Zeit verlangt -- sein Body, Swap zu
  Swap, in ganzen Blöcken bei seinem Tempo (der Bruchteil gezogen), dazu Intro und Outro von je 32 Takten unter den
  Nachbarn. Rund 20 Tracks die Stunde. Sein Kick-Out bleibt ein Moment: höchstens 8 Takte bei drei Body-Blöcken, 16 bei
  vier (ein Peak-Kick-Out von 32 Takten nahm sonst den ganzen mittleren Block und lag 36 dB unter dem Set); die Hat-Welle
  über einem Kick-Out lässt die Hats offen.
- **Der Blend** nach dem "Ablauf Takt für Takt" (Mix-Dok. 7): der Fader des Kommenden öffnet `set.blend` Takte vor dem
  Swap, seine Höhen 8 dB unten und über ein Viertel des Blends zurück, seine Mitten 18 dB unten und über die letzten 16
  Takte zurück; der Bass des Gehenden 6 dB herunter über dieselben 16 Takte ("A Low auf ~11 Uhr"), dann der Swap im
  selben Sample; der Gehende behält seine Hats (Mitten fallen in 8 Takten, Höhen über 16), sein Fader fällt durch die 16
  danach; mit `set.fx_breaks` geht sein Rest ins Echo.
- **Deck C borgt** (Mix-Dok. 6: "Zwei bis drei Tracks laufen ständig ... einzelne Elemente werden aus einer Platte
  geborgt"): je Track-Strecke eine Anleihe (p `set.loops` × 1,4, Standard jetzt 0,5) -- das **Carry** (Hats und
  Percussion des gerade gegangenen Tracks laufen 32 oder 48 Takte weiter), der **Tease** (die Figur des nächsten Tracks
  16 oder 32 Takte vor seinem Blend, hochpassgefiltert und sich öffnend, nur wo die Tonarten gleich oder eine Quinte
  auseinander sind), der **Layer** (ein Perc-Loop des vorletzten Tracks 16 oder 32 Takte unter dem Kern). Immer eines
  zugleich, 16 Takte Nachhall dazwischen.
- **Die Hand des DJs** (`set.dj_hand`, 0,5; Mix-Dok. 6: "working the EQs, the effects, all precision"): zwischen den
  Blends auf den 16-Takt-Linien des Tracks ein Low-Kill (1 oder 2 Takte vor der Linie, die Kick schlägt zurück), ein
  High-Swell (Höhen −10 dB, zurück über 8 oder 16 Takte: Mills' "Rides am Mixer eingefadet"), ein Mitten-Dip, ein
  Hochpass-Build in die Linie, ein Echo-Throw auf dem letzten Schlag -- nie näher als vier Takte an den eigenen Momenten
  des Tracks (Phase 8), damit die zwei Hände nichts doppeln.
- `tot_render --plan` nennt die Anleihen nach Art, die Bewegungen der Hand und wie viel des Sets geschichtet ist; die
  Cues heißen "Carry of T3", "Tease of T5", "Layer of T2". Neuer Test `testMix`.

Gemessen an einem Set von 30 Minuten (Seed 2026, Peak; `out/p9`, vorher mit dem Renderer von Phase 8): vorher 7 Tracks,
2 Loops, 1 Break; jetzt 10 Tracks (19,2 die Stunde), 5 Anleihen, 11 Handgriffe, zwei Quellen oder mehr in 62 % der
Takte, drei in 7 %. Wechsel je 64 Takte (meso) 3,3 → 4,9; die Lautheitsspanne bleibt bei 3,7 LU (ohne die
Kick-Out-Grenze waren es 6,7: der 32-Takt-Kick-Out eines Peak-Tracks lag 36 dB unter dem Set). Die tiefsten Stellen sind
jetzt die Kick-Outs von 8 Takten. `selftest` 176/176. Offen (Schritt 3): die Kalibrierung an echten Mixen (Tracklists von
Berghain 04 und 06: Tracks je Stunde, Überlagerung, Tempodrift) und Hörrunden.

**28.09.2026, Vormittag: Phase 8, Figur und Spannung.** Nach dem ersten Hören ("es gab eben keine richtigen Ups und
Downs, es plätscherte eben einfach so vor sich hin in einem starren 32-Takt-Raster") und mit dem zweiten
Recherchedokument des Nutzers (`Berlin Techno: Arrangement und DJ-Mixing im Club`, 28.09.2026; im Folgenden **Mix-Dok.**).
Befund vorher, gemessen: Im Body änderte sich einmal je 32 Takte etwas, meist Hats oder Percussion; die Dichte stieg
einmal treppenförmig auf "voll" und blieb dort; tonal spielten Hypnotic-Tracks oft nur den Ping (Seed 7: in 39 % der
Takte) und eine Drone (2 %). Ein neues Maß in `Tools/analyze_ref.py` (`meso_*`): die Bandenergien tief, mittel und hoch
über acht Takte geglättet, ihre Spanne über den Body und die **Wechsel**, Stellen, an denen ein Band über acht Takte um
mindestens 2 dB anders liegt als in den acht davor, je 64 Takte. Die 30 Referenzen: Median **4,2** Wechsel (10.–90.
Perzentil 1,5–6,6), die Höhen (5–10 kHz) bewegen sich um 5,9 dB. Neun der Kalibriertracks von 1.0.0: Median **1,5**,
einer 0, einer 0,5, die Höhen im Median um 1,3 dB.

- **Die Figur** (`RackPlan::figure`, `makeFigure`, Mix-Dok. 2: das A-Moll-Motiv von "The Bells", die Bassline von "Energy
  Flash", die Dub-Chords von "Dawning"): jeder Track hat eine Stimme, an der man ihn wiedererkennt, nach dem Stil gewählt
  (Hypnotic vor allem der Ping, Ostgut Stab und Bass, Dub der Akkord, Raw die 303; der Bass nur, wo der Sub den Bass
  besitzt; monoton: keine). Sie spielt ein Motiv von ein oder zwei Takten, vom Einsatz bis zum Ende gleich (Myloops'
  Anker), und bewegt sich nur über ihre Filter: die 303 eine Linie mit Akzenten und Slides, deren zweiter Takt antwortet;
  der Akkord einen Stab-Rhythmus des Dub-Idioms (Offbeat-Achtel, Dok.-8.2-Stab, 3-3-2, ein langer Akkord je Takt,
  Frage und Antwort über zwei Takte, E(5,16)); der Bass ein Riff neben den Vierteln; der Ping einen Zyklus bis drei
  Töne. Sie setzt im zweiten Body-Block ein (Mix-Dok.: "+Stab bei 65"), der Bass mit dem Body; sie ist immer im Pool und
  gibt bei der Harmonieregel ihre Töne zuletzt ab. Eigener Strom `figure`: ein Reroll zieht ein anderes Motiv für
  dieselbe Stimme. Ihr Akkord ist um 70 % dessen leiser, was er länger klingt als die alten Stabs (sonst wurden die
  Dub-Tracks um 4 dB breiter).
- **Wellen** (Mix-Dok. 4 und 5, "Automation auf drei Zeitskalen"): vom Einsatz der Figur bis zum Outro läuft der Body in
  Wellen von 32 oder 64 Takten; die Filter der Figur und der Hat-Bus (Tiefpass bis 2,9 kHz, Pegel bis 5 dB) öffnen
  zur Landung hin und fallen danach zurück -- ein Kriechen bis zur Landung, ein Bogen über die Mitte oder ein schnelles
  Öffnen, das langsam schließt. Zwei bis vier Takte vor einer Landung fällt etwas weg, damit sie landet: die Mitte (The
  Acid Mind: "removing the center"), Kick und Claps (Mills), die Figur, oder die Kick für einen Takt; vor der
  Hauptlandung (nahe 60 % des Bodys) die Mitte oder die Figur, dazu mit einer Stil-Chance ein Noise-Swell über acht Takte.
  In der Mitte einer Welle atmet ein Element 8 oder 16 Takte aus und kommt mit einem Delay-Throw zurück (The Acid
  Minds 64-Takt-Zelle): die Figur, der Bass oder das Rumble, die Percussion, die Tops (Open Hat, Ride, rollende Hat).
  Die Anteile je Stil sind an der Referenzbewegung gemessen: Hypnotic bewegt sich unten und oben, kaum in der Mitte;
  Raw oben, bei flacher Lautheit; Ostgut und Dub überall.
- **Das Raster weicher:** Eine Hat oder Percussion setzt auf Takt 1, 9 oder 17 ihres Blocks ein (0,5 / 0,3 / 0,2; Mix-
  Dok.: "jeder Einsatz sitzt auf Takt 1 einer 8-Takt-Phrase"), Bass, Figur und große Wechsel bleiben auf der 32. Die
  Grammatik heißt jetzt: eine Operation je Block, auf seinem ersten Takt oder als solcher Einsatz auf seiner 8- oder
  16-Takt-Linie (`testComposer`); mindestens zwei Einsätze in der zweiten Hälfte, auch wenn die Figur früh kam.
- **Anzeige:** `tot_render --plan` nennt die Figur, die Landungen und die Momente dazwischen; die Arrange-Seite Figur und
  Landungen; neuer Reroll-Knopf "figure".

Gemessen an denselben zwölf Tracks (`out/p8/phase8d`, `docs/eval/kalibrierung-phase8.md`): Wechsel je 64 Takte im
Median **3,8** (Referenzen 4,2): Hypnotic 4,0 (4,5), Ostgut 5,0 (4,6), Dub 3,0 (3,7), Raw 3,5 (3,3); die Höhen bewegen
sich um 6,3–7,1 dB (5,9). Die ganze Kalibrierung: 12 von 96 Werten außerhalb des 10.–90. Perzentils der Referenzen
(vorher 14); die Taktähnlichkeit hält den Korridor (1 außerhalb, vorher 3). Offen: Hypnotics Mitten bewegen sich mehr
als die Referenzen (4,6 dB gegen 2,6; schon vor Phase 8 so), Raws Lautheit bleibt etwas breiter (LRA 2,4 gegen 1,3).
`selftest` 172/172 mit dem neuen `testFigure`; `ctest` 32 von 32 ohne pluginval.

**Weiter (Mix-Dok. 6--8, vom Nutzer als Ziel genannt: "ein langer Mix, der automatisiert generiert wird"):** Schritt 2,
die Mix-Engine als Live-Rekomposition -- drei Decks durchgehend, von jedem Track vor allem die Kernblöcke hörbar
(etwa drei Minuten, die Hälfte überlagert), Elemente geborgt (die Hat von hier, der Perc-Loop von dort), eine DJ-Hand
an EQ, Filter und Effekten auf drei Zeitskalen, der Set-Bogen nach Klock und Nodge. Schritt 3, die Kalibrierung an
Mixen (Tracklists von Berghain 04 und 06: Tracks je Stunde, Überlagerung, Tempodrift) und Hörrunden.

**28.09.2026, morgens: Totality.** Der Nutzer hat den Generator umbenannt, weil "Umbra" als Audio-Software vergeben
ist (Risiko 7). Alles heißt jetzt so: Plugin, Standalone und Installer (`Totality.vst3`, `Totality.exe`, neue AppId,
damit er nicht in den alten Ordner installiert), der Kern `TotalityCore`, Namensraum `tot::`, Präfix `TOT_` (auch die
Umgebungsvariablen), `tot_render`, Set-Dateien `.totset`, die OSC-Adressen `/tot/beat` ... `/tot/key`, das Quest-Paket
`com.reneweller.totality.quest` (`TotalityQuest.apk`), das Handbuch, die Update-Prüfung (`reneweller-coding/Totality`),
der Ordner der Referenz-Tonspur (`%TEMP%\totality_refs`). Der Plugin-Code ist `Totl`: ein Host sieht ein neues
Instrument, Projekte mit dem alten laden es nicht. Geblieben ist das Wort als Begriff: die Kick-Scheibe des
Eclipse-Blicks heißt im Code weiter die Umbra, und "Umbra" ist eines der Nomen der Sub-Gruppe "Dark Sub".

Dazu das Logo, gewählt aus sechs Vorschlägen (Korona, Diamantring, sechzehn Steps, Wortmarke, Kontaktphasen, das
bisherige Icon): **die Korona** (`Deploy/make_icon.py`). Die schwarze Mondscheibe mit dem hellen Rand der Chromosphäre
und die Korona in 48 Strahlen, lang am Äquator, kurz an den Polen, wie sie im Sonnenminimum steht; bis 32 Pixel 24
Strahlen, dicker und kürzer. Dieselbe Zeichnung ist das Icon (`.ico`, Installer, Launcher-Icon der Quest in fünf
Dichten, jetzt aus demselben Skript), der Kopf des Plugins (`drawLogo`), das Logo auf dem Quest-Panel in Lichtpunkten
(`addLogo`), das Deckblatt des Handbuchs und die Social Preview. Screenshots, Handbuch, APK und Release neu gebaut.

**28.09.2026, später in der Nacht: Presets.** Auf die nachgereichte Bitte ("Presets für die einzelnen Synth, ähnlich wie
für Ephemeris ... 1024 pro Synth-Engine ... vom Composer ausgewählt und beim Abspielen angezeigt ... Knöpfe/Encoder/
Mixer-Züge auf die absoluten Preset-Werte setzen").

- **Die Bibliothek** (`Presets.h/.cpp`, Bauweise aus Ephemeris): 1024 je Engine für Kick, Rumble, Sub, eine Kit-Lane,
  Ping, Bass, 303, Dub-Chord, Drone und Textur, zusammen 10 240 mit eindeutigen Namen. Je Engine sechzehn Gruppen
  ("Berlin 909", "Hypnotic Thud", "Dub Sine", "Raw Distorted"; "Berghain Roll", "Cathedral Low"; "909 Closed Hat",
  "Ride Bell", "Rimshot", "Conga Skin"; "Classic 303", "Screamer"; "Basic Channel", "Dub Chord", "Tape Chord" ...) zu je
  64 auf einem 8×8-Raster: Adjektiv von dunkel nach hell (die Helligkeitsachse: Ton, Cutoff, Band), Nomen der Gruppe
  (die Formachse: Decay, Resonanz, Drive), der Rest gezogen. Ein Preset ist der Klang, nicht der Mix: Pegel, Pan,
  Sends, Ducking, Oktaven, die Rolle und die Pattern-Knöpfe einer Lane bleiben. Jede Gruppe sagt, wie gut sie zu
  Hypnotic, Ostgut, Dub und Raw passt; Kit-Gruppen nennen ihre Rollen (`testPresets`).
- **Die Wahl des Komponisten** (`compose.pick_sounds`, Strom `sounds`): je Track und Synth eine Gruppe nach ihrer
  Passung zur Stilmischung des Profils (kubisch gewichtet; `StyleProfile::styleMix` morpht mit), eine Lane nur unter
  den Presets ihrer Rolle, in der Gruppe die Reihe dreiecksverteilt zur Mitte (die hellsten und dunkelsten seltener).
  Die Rezepte der Stile sind jetzt nur noch Mix; was sie an Klangknöpfen setzten, ist ein Bereich, in dem der Stil den
  Preset-Wert hält (Dubs weicher Klick und heller Chord, Raws harter Clip), wo er ihn nicht ohnehin trifft.
- **Absolute Werte je Deck** (`Score::knobs`, `KnobSet`): alles, was ein Track an seinem Anfang setzt -- Presets, Mix des
  Stils, Tonart --, ist kein Gesten-Offset mehr, sondern ein absoluter Wert. Jedes Deck spielt aus den Werten seines
  Tracks; die Engine schreibt die Werte des zuletzt begonnenen Tracks (Deck A oder B) auf die Knöpfe
  (`Engine::soundsVersion`, das Plugin sagt es dem Host), und wo eine Hand einen Knopf von dort wegdreht, folgt der Klang um
  so viel. Im Blend zeigen die Knöpfe den kommenden Track, das gehende Deck spielt weiter seinen (`testKnobs`).
  Automationsgesten laufen relativ zum Wert des Tracks.
- **Anzeige:** jede Synth-Seite hat oben ein Preset-Feld (Menü der 1024 in ihren Gruppen, zurück/vor, "this track:
  Punchy Drum Hall (Hypnotic Thud)"), die Arrange-Seite listet die Sounds des Tracks, die Quest zeigt Kick und Ping,
  `tot_render` druckt alle.

Die Kalibrierung mit Presets (dieselben 12 Tracks wie in Phase 4): 10 bis 14 von 96 Werten außerhalb des 10.–90.-
Perzentils der Referenzen, je nach gezogenen Presets (vorher 10; bei gleicher Verteilung wären etwa 19 zu erwarten).
Zwei Befunde behoben: Die Textur-Presets drehten Knistern und Brummen bis +12 dB über den Standard -- ihre Mengen sind
jetzt so bemessen, dass die Textur etwa so laut bleibt; und beim Peak nahm der Komponist den Block nach der Rückkehr als
den lautesten, obwohl danach noch Lagen einsetzen -- jetzt den dichtesten ab der Rückkehr. Raw bleibt am Rand: breiter
und dynamischer als die schmalen, flachen Referenzen (schon vor den Presets knapp).

`ctest` 31 von 31 (dazu pluginval Strenge 10: bestanden), `vst3test` 30/30, APK neu gebaut.

**28.09.2026, nachts: Phasen 5 bis 7, Plugin, Quest, Release 1.0.0 (lokal).** Auf "Bitte baue die restlichen Phasen
und was noch fehlt nacheinander in dieser Nacht" hin, ohne Agenten; alles lokal committet, nichts veröffentlicht.

Was vorher fehlte:
- **OSC-Cues für Kaleidoscope** (10.3): `Cue.h` aus Ephemeris, Totalitys Marken: `/tot/beat`, `/tot/bar`, `/tot/block`,
  `/tot/op`, `/tot/key` (Camelot); der Audio-Thread stempelt, ein eigener Thread sendet (`testCues`: Bytes nach OSC 1.0,
  Marken, Tap, ein Datagramm durch das Loopback).
- **Stems, deren Summe die Mischung ergibt**: jeder Stem durch eigene Kopien der linearen Stufen (Gruppen-Hochpass, Tilt,
  die Frequenzweichen der Ducks, Isolator und Filter des Mixerkanals) und mal den Gains der nichtlinearen (Drum-Bus-
  Sättigung, Glue, Trim); die Mixer-Effekte als eigener Stem `djfx`. Die Summe ist die Mischung vor dem Master auf
  −120 dB genau, in einem Track, in einem Blend mit Bass-Swap und in einem Break; mit Stems ist die Mischung bitgleich
  zu ohne (`testStems`).
- **Perform und MIDI-Learn**: `perform.*` (Master-Filter, Echo-Wurf, sieben Mutes: Kick samt Rumble, Sub, Hats, Perc,
  Ping, Bass samt 303, Pads), wirksam nur live (`Engine::setLive`: Plugin und Quest; Renders und Exporte spielen die
  Partitur, wie sie komponiert ist). Ein Mute lässt die Noten weg, die Fahnen klingen aus (`testPerform`).
- **Style-Tab**: `custom.*`, ein eigener Stil über dem Profil (Tempo, Formgewichte, Länge, Rack, Swing, Ränder, längster
  Kick-out, Ereignisse, Korridor, Ziel-LUFS), dazu die Zahlen des Profils und die Referenz-Mediane.
- **Namensprüfung (Risiko 7):** "Umbra" ist als Audio-Software **vergeben**: Orchestral Tools vertreibt ein
  kommerzielles "Umbra" (VST/AU/AAX, Sample-Instrument), H.G. Fortune ein freies "Umbra"-VSTi (2008), dazu
  ein "Umbra VSTi" für Trap. Eine erste Suche nach "Totality" fand keine Audio-Software (keine Markenrecherche).
  **Der Nutzer hat entschieden (28.09.2026): der Generator heißt Totality.**
- **Rekordbox/Traktor**: die Prüfung der Cue-Marken in DJ-Software bleibt beim Nutzer (15.8); JSON-Cues liegen daneben.

**Phase 5, GUI** (`Plugin/`, Gerüst aus Ephemeris): Tabs nach 10.1 -- Set, Arrange (Zeitleiste, Operationen, Matrix je
Lagengruppe, Rerolls des Tracks unter dem Abspielkopf), Patterns (die **Eclipse**: die Kick als dunkle Scheibe, jede
Stimme ein Ring von Perlen, ein Takt je Umlauf, Polymeter präzedieren, Konjunktionen ab drei Ringen als Strahl; daneben
das Step-Raster mit Off-Grid-Strich), Low End, Drums (zwölf Lanes), Tones, Dub, Mixer (Meter je Deck, Ausgang, Lautheit
über 400 ms K-gewichtet), Perform (Mute-Pads, Master-Filter, Wurf, Kills und Fader je Deck, Learn an jedem Regler,
Rechtsklick vergisst; Tasten C3 bis F#3 schalten die Mutes, Modrad Filter, Expression Wurf), Export (WAV mit Cues, MIDI,
Stems, DJ-Loops, `.totset`, OSC), Style. Die Parameterseiten entstehen aus den Tabellen (`EditorTheme.cpp`, `layoutOf`).
Im Host gilt das Host-Tempo, Sprünge des Playheads werden verfolgt. `TOT_SHOT`/`TOT_TAB`/`TOT_SHOT_FULL`/`TOT_SET` für
Bilder ohne Menschen; Update-Prüfung wie Ephemeris (`reneweller-coding/Totality`).
Prüfsteine: `vst3test` 30 von 30 (Tempo, Sprung, MIDI, Zustand, 44,1/96 kHz, zweite Instanz, Editor), **pluginval
Strenge 10 bestanden**.

**Phase 6, Quest** (`Quest/`, Rahmen aus Ephemeris' Quest-App): der ganze Generator auf dem Gerät, live; die Hände am
Mischpult -- links Pinch Play/Stop, rechts Pinch Kick-out/-in, beide zusammen der nächste Track, Handhöhe links
Master-Filter, rechts Echo-Wurf; ein Pinch wirkt beim Öffnen, damit beide zusammen nicht zugleich zwei einzelne sind.
Die **Eclipse im Raum**: 3 m voraus, 1,3 m über den Augen, zum Spieler geneigt, am Sessionbeginn verankert; Korona
schwillt mit jeder Kick, Perlen des nächsten Takts blenden über das erste Achtel ein (keine Sprünge, Kaleidoscope-Regeln).
`tot.cfg` mit Seed, Stil, Set-Länge, Qualität, OSC, beliebigen Knöpfen. APK gebaut (4,8 MB, arm64, ohne Warnung),
**auf dem Gerät ungetestet** (kein Headset angeschlossen).
Qualitätsstufe `Engine::Quality::Quest`: Granularwolke aus, der Rumble clippt ohne Überabtastung. Kosten je Stufe mit der
CMake-Option `TOT_PROFILE` gemessen (20-Minuten-Set, Seed 2026, Desktop): Rumble 12 %, Kit 13 %, Dub-Kette 11 %,
Raum 9 %, Master-Clipper 7,5 %; gesamt 7,8 % eines Kerns, in der Quest-Stufe 7,5 %. Auf der Quest 2 wären das mit dem
üblichen Faktor 3 bis 4 etwa 23 bis 30 % -- am Rand des Plans; messen kann es nur das Gerät. Weitere Hebel (nicht gebaut):
das abgehende Deck eines Blends in der niedrigen Stufe (11), ein kleinerer Raum.

**Phase 7, Qualität und Release:** Version 1.0.0. Handbuch aus dem Programm (`Tools/manual`: `tot_render --dump-params`
für die Tabellen, die Screenshots aus dem Standalone, der Text in `chapters.txt`; PDF über Edge), Social Preview
(`docs/social-preview.png`), Installer (`Deploy/Totality.iss`, Inno Setup) und `Deploy/build_release.ps1` (icx,
statische Laufzeit, Tests, pluginval, Handbuch, Prüfung der Abhängigkeiten, Prüfsummen, portables Zip, Setup): der lokale Durchlauf mit icx bestand alle 30 Tests und pluginval, keine Laufzeit-DLL, Setup 10,3 MB und
portables Zip 11,8 MB in `Deploy/out`.
Nicht getan, bewusst: Hörrunden (des Nutzers), Nachkalibrierung (die offenen Befunde der Phase 4 -- Endless flach, Dub am
unteren Rand des Schwerpunkts -- bleiben), Veröffentlichung auf GitHub (morgen, durch den Nutzer).

| Prüfstein | Ergebnis |
|---|---|
| Standalone und VST3 bedienbar | 11 Tabs, Screenshots aller Seiten (`docs/screenshots`), `vst3test` 30/30 |
| pluginval Strenge 10 | bestanden |
| Quest-APK | gebaut; Gerätetest und CPU-Messung offen |
| `ctest` | 29 von 29 (dazu pluginval: 30); mit den Presets 31 von 31 |

**27.09.2026, spät: Phase 4, Komponist und Set.** Der Komponist (`compose/Composer`) schreibt Tracks in den drei Formen von
7.2 aus vier Stilprofilen (`compose/Style`, designierte Initialisierer wie Ephemeris, Morph und die Achsen Dub/Hypnotic);
der Set-Komponist (`compose/Set`) mischt sie auf zwei Decks. Die Engine besteht jetzt aus drei Decks (`Deck`: alle
Stimmen bis zum Track-Bus mit Tilt, Glue und Leveler-Trim, eigene Partitur und Automation) und dem DJ-Mixer (Fader,
Isolator 200 Hz/2,5 kHz mit Kill, bipolarer Filter, FX-Send in Tempo-Echo und Hall) vor dem Master (Mono unter 120 Hz,
Level, Cut, Clipper, Limiter).

- **Form:** Dichteprofil je Block (Tool 0,3 → 0,6 → 0,85 → 1,0, das Outro 0,6 → 0,3; der Peak voll nach der Rückkehr),
  genau eine Operation je Block, Einsatzreihenfolge nach Dok. 8.5 (der Sub-Bass mit dem Body), die letzten zwei Lagen
  erst in der zweiten Hälfte. Der Vorrat eines Tracks ist so groß, wie Blöcke da sind, ihn einzuführen (Intro-Perc und
  -Hat, je Body-Block einer); die wahrscheinlichsten Lagen eines Stils (seine Signatur) bleiben. Reduktion als Kick-out
  mit Rückkehr auf einer 16/32-Linie, Swell, Cut, Wolke, Hochpass-Fahrt. Das Endless beginnt mit Kick, Bass, Tops und
  der Signatur seines Stils (Dubs Akkord, Hypnotics Ping).
- **Ereignisse** auf den 8-Takt-Linien: Mute-One-Hit, Ein-Takt-Dropout, Delay-Wurf (Ping, Stab oder Hats ins Echo),
  ein Ghost mehr; Gruppen-Hochpass vor 32-Linien und Rückkehren.
- **Automation:** Mikro (Hat-Decays), Meso (zwei Hände aus Ephemeris' `GestureEngine`, neu auf einem 16-Beat-Raster:
  Filter und Sends dessen, was spielt, ihr Zentrum folgt der Dichte), Makro (Rumble-Hall, Stab-Helligkeit +20 %).
- **Harmonik** nach Dok. 8.6 und die Prüfung von 8.9 als Filter: höchstens vier Tonhöhenklassen (der Shuttle, der
  zweite Ping-Ton und das 303-Alphabet weichen in dieser Reihenfolge), der Bass höchstens zwei; Camelot-Label.
- **Kandidaten:** acht je Block (Varianten der Rack-Würfe), gewählt nach dem Abstand zum Korridor des Profils
  (`compose/Corridor`: Taktähnlichkeit bei Abstand 1/2/4, Mikroveränderung, Dichte, auf der Partitur wie `analyze_ref.py`
  auf dem Audio).
- **Kuration:** jeder Teil auf eigenem Strom (`form`, `harmony`, `rack`, `rack.<layer>`, `layers`, `blocks`, `block<n>`,
  `events`, `hands`, `sounds`; im Set `track<n>` und `track<n>.<unit>`, `set`); `.totset` (aus Ephemeris) hält Seed,
  Längen, Rerolls und geänderte Knöpfe.
- **Set:** Dramaturgien Warm-up (125 → 130), Peak (128 → 134), Closing (132 → 127), Sunday (126 → 128), Flat (130),
  höchstens 1 BPM je Track, die Rampe im Blend; die Stile wandern mit der Energie über Dub, Hypnotic, Ostgut, Raw, um den
  eingestellten Stil zentriert; Tonarten als Camelot-Nachbarn; Blend 16/32 Takte, harter Bass-Swap auf der 32-Linie
  (beide Decks im selben Sample); Live-Loops auf Deck C; Breaks über die Mixer-Effekte.
- **Ausgaben:** Cue-Marken in der WAV (`cue `/`LIST adtl`) und als JSON, nahtlose DJ-Loops von 4 und 8 Takten (dreimal
  gerendert, der letzte Durchgang behalten) mit Kick, Hats und Perc allein, Set-MIDI mit Tempo-Drift und Markern,
  Stems (je Element über die Decks summiert), Deck-Abgriffe, die Partitur als JSON.

Prüfstein von Abschnitt 14:

| Prüfstein | Ergebnis |
|---|---|
| Zwei-Stunden-Set aus einem Seed | 27 Tracks in 121 Minuten (Seed 2026, Peak: 128 → 134 BPM, Hypnotic → Ostgut), 6 Live-Loops, 8 Breaks, Cues, MIDI, `.totset`; 11-fache Echtzeit, 9,3 % eines Kerns (zwei Decks in den Blends); −10,1 LUFS, True Peak −0,99 dBTP, lautestes Kurzzeitfenster −8,1 LUFS ([Bericht](eval/set-seed2026.md)) |
| Determinismus | derselbe Seed, dasselbe Set und derselbe Track; ein Block, eine Lage, die Hände einzeln neu, der Rest bitgleich (`testCuration`, `testSet`) |
| Blend-Test | nur ein Deck besitzt je das Tiefband (1 408 Takte eines 40-Minuten-Sets geprüft), Bass-Swap in einem Sample, zwei Decks bitgleich über Blockgrößen (`testSet`) |
| Grammatik | 32 Tracks: je Blockgrenze genau eine Operation, alles auf 4-Takt-Linien, Rückkehr auf 16-Takt-Linien, nichts Tonales in den ersten und letzten 32 Takten, Harmonik nach Dok. 8.9 (`testComposer`) |
| Evaluationsbericht | `Tools/eval_report.py`; Kalibrierung (12 Tracks, 4 Stile × 3 Seeds): 10 von 96 Werten außerhalb des 10.–90.-Perzentils der Referenzen (bei gleicher Verteilung wären es etwa 19); Set: Set, jeder Track vom Swap bis zum nächsten gemessen: Form und Harmonik ohne Verstoß in 27 von 27 Tracks; außerhalb des Bands Schwerpunkt 0, Breite 1, LUFS 0, lautestes 20 s 0, Takt-Ähnlichkeit 5, Mikroveränderung 0, Sub-Anteil 7 von 27 ([Kalibrierung](eval/kalibrierung-phase4.md)) |
| `ctest` | 25 von 25 |

Kalibrierung gegen die Referenzen, drei komponierte Tracks je Stil (Median der drei; Referenz-Median):

| Größe | Hypnotic | Ostgut | Dub | Raw |
|---|---|---|---|---|
| Schwerpunkt, Hz | 288 (252) | 277 (331) | 107 (182) | 235 (170) |
| Breite S/M, dB | −5,0 (−4,5) | −7,7 (−6,2) | −10,0 (−7,9) | −6,4 (−8,1) |
| LUFS / lautestes 20 s | −10,9 / −9,9 (−11,5 / −10,0) | −10,6 / −9,1 (−10,8 / −9,5) | −12,2 / −11,0 (−12,2 / −11,5) | −10,1 / −9,4 (−9,9 / −9,4) |
| LRA, LU | 3,6 (4,7) | 4,0 (3,0) | 3,0 (3,4) | 2,1 (1,2) |
| Takt-Ähnlichkeit | 0,88 (0,87) | 0,957 (0,955) | 0,917 (0,931) | 0,950 (0,924) |

Befunde, alle von Tests oder Messungen gefunden:
- **Balance und Breite hängen am Fenster.** `analyze_ref.py` misst sie auf der mittleren Minute; bei einem
  Peak-Track liegt dort der Kick-out (Schwerpunkt 3,6 kHz, nur Hats). Totalitys Tracks werden jetzt auf ihrer lautesten
  Minute gemessen (der Body voll, wie die mittlere Minute einer Referenz meist), die Referenzen wie bisher.
- **Die Rezepte der Stile** sind auf der lautesten Minute von je drei komponierten Tracks gefittet (Tilt Hypnotic 12,
  Ostgut 12,5, Dub 13, Raw 4,5 dB; Raum, Busse, Flächen, Ping und Echo je Stil). Dub braucht helle Stabs (1–5 kHz) und
  sehr leise Hats, Raw kaum Tilt und viel Raum.
- **Der Raum explodierte an der Drone**: Die +22 dB des Raums sind an Hits gefittet (Hats, Perc, Ping); eine gehaltene
  Fläche füllt das FDN weit stärker, bei Send 0,2 lag der Raum 6 dB über der Drone selbst (ein Set kam kurzzeitig auf
  −4,3 LUFS). Drone- und 303-Send auf ein Zehntel.
- **Die Lautheit bewegt sich an den Rändern**: Die Referenzen liegen im Intro 4 bis 9, im Outro 5 bis 19 dB unter dem
  Body, Totalitys Tracks lagen 1 bis 4 darunter (LRA 1–2 statt 3–5). Das Rumble kommt jetzt mit dem Body (p 0,7), und die
  Ränder laufen durch den Gruppen-Hochpass (die Fahrt aus Dok. 8.5s Tabelle; je Stil p 0,1 bis 0,6).
- **Raw kürzt**: Die Raw-Referenzen haben 0,5 Reduktionen je Titel, Median 3 Takte; ein Kick-out von 32 Takten gab LRA
  4,7 statt 1,2. Die längste Reduktion ist jetzt eine Größe des Profils (Raw 8 Takte).
- **Der Korridor lag unerreichbar tief**: Die Taktähnlichkeit der Partitur liegt 0,04 bis 0,07 über der des Audios; mit
  Zielen unter dem Erreichbaren wählten die Kandidaten blind. Ziele jetzt Referenz-Audio + 0,05.
- **Kuration und Kandidaten**: Variierten die Kandidaten auch die Operation, änderte ein neu gewürfelter Block alles
  danach. Operationen und Ereignisse kommen jetzt aus eigenen Strömen, die Kandidaten variieren nur die Würfe.

Abweichungen vom Plan, bewusst:
- **Kandidaten variieren nur die Würfe** (7.9 nannte Op, Ereignisse und Motion-Seeds): sonst hielte die Kuration nicht.
- **Dur (0,04) entfällt** (Totalitys Skalen sind Moll-Modi); der Tonartwechsel in einem Track (p 0,04) ist nicht gebaut.
- **Klänge als Rezept plus gezogene Bereiche je Track**, noch keine Preset-Bänke als Programmwechsel (Phase 5/7).
- **Das Glue sitzt im Track-Bus jedes Decks**, nicht im Master: So kommt der Leveler-Trim weiter hinter dem Glue.
- **Endless nicht im Hauptdeck eines Sets**; Deck C trägt die Loops.

Offen: der Hörvergleich je Profil und des Sets durch den Nutzer; die Prüfung, ob Rekordbox und Traktor die Cue-Marken
der WAV lesen (15.8); die OSC-Cues für Kaleidoscope (Phase 5); Stems, deren Summe die Mischung exakt ergibt.

**27.09.2026, nachts: Phase 3, Klang und Mix.** Neu sind der Bass-Synth und die 303-Linie (eine Stimme: PolyBLEP bei
2×, Sub-Rechteck, die Schaltungsfilter aus Ephemeris, Akzent, Slide, Glide), der Dub-Chord (acht Stimmen, je zwei
Sägezähne, Bus mit Bandpass-Fahrt, Bitreduktion, vierstufigem Phaser, Senke bei 600 Hz), Drone und Textur (Knistern,
Netzbrummen, Rausch-Erosion), die Dub-Kette (Bandecho mit Federn, Platte; aus Ephemeris), der Multiband-Duck für Flächen
und Rückwege, die Granularwolke, drei Sättigungsstufen auf dem Drum-Bus, der Master mit parallelem Glue, Tilt,
Vinyl-Schnitt, Clipper bei 4× und True-Peak-Limiter, und der Leveler (aus Ephemeris). Die Studie bringt je Stil eine
eigene Reihenfolge der Layer, Delay-Würfe am Phrasenende, die Treppe des Akkords ("nudging louder"), Filterfahrten von
Stab und 303 und die Wolke durch die Reduktion. Die Harmonik folgt Dok. 8.6 als harte Regel (Shuttle-Akkorde als
Dreiklänge in den Terzen der Tonart, add9 nur, wo die Skala die None hat).

Gemessen an zehnminütigen Studien (Seed 7; Dub mit Sub-Besitz) gegen die Mediane der Referenzen, mit demselben Werkzeug:

| Größe | Hypnotic: Totality / Ref. | Ostgut | Dub | Raw |
|---|---|---|---|---|
| Bänder dB gegen 40–140 Hz: 250 Hz–1 kHz | −8,6 / −12,8 | −14,7 / −12,0 | −14,2 / −13,4 | −14,8 / −13,0 |
| 1–5 kHz | −15,1 / −15,8 | −16,9 / −15,4 | −23,7 / −17,9 | −18,2 / −18,5 |
| 5–16 kHz | −18,7 / −17,4 | −17,8 / −16,3 | −20,4 / −27,7 | −17,8 / −20,7 |
| Schwerpunkt, Hz | 264 / 252 | 247 / 331 | 164 / 182 | 238 / 170 |
| Seite zu Mitte über 200 Hz, dB | −1,9 / −4,5 | −9,9 / −6,2 | −8,1 / −7,9 | −10,8 / −8,1 |
| lautestes 20-s-Fenster, LUFS | −9,8 / −10,0 | −9,5 / −9,5 | −11,0 / −11,5 | −9,5 / −9,4 |
| integriert, LUFS | −11,5 / −11,5 | −10,0 / −10,8 | −11,9 / −12,2 | −10,0 / −9,9 |
| LRA, LU | 2,7 / 4,7 | 1,1 / 3,0 | 2,3 / 3,4 | 1,1 / 1,3 |
| Takt-Ähnlichkeit | 0,87 / 0,87 | 0,94 / 0,955 | 0,90 / 0,93 | 0,89 / 0,92 |

Vorher (Phase 2) lagen 1–5 kHz 9 bis 13 dB und 5–16 kHz bis 10 dB zu tief, der Schwerpunkt bei 96 bis 129 Hz, die
Breite bei −13 bis −27 dB. True Peak −1,0 dBTP überall (die Referenzen liegen über 0, das wird nicht nachgeahmt), Crest
9,5 bis 11,2 dB. `ctest` 22 von 22; Render 17-fache Echtzeit, 5,8 % eines Kerns.

Befunde, alle von Tests oder Messungen gefunden:
- **Die Balance ist ein Tilt, keine Summe von Einzelpegeln.** Ein Ausgleich der Stems gegen Bänder und Breite der
  Referenzen (kleinste Quadrate über alle vier Stile, `kMixParams` in Params.cpp) fand für jeden Stil dasselbe: die ganze
  obere Hälfte des Spektrums rund 10 dB zu tief gegen das Tief. Master-Tilt +10 dB (Erg. 2: der Standard folgt aus dem
  Abstand zu den Referenzen), Raum +10 dB, Akkord und Drone +7, Hats −3,5, Perkussion −6,5, Ping +1,5. Der Tilt hebt die
  Höhen gegen das Tief und lässt das Tief stehen; um 1 kHz gekippt nahm er dem Tief 5 dB und mit ihnen den Pegel.
- **Der Leveler kämpfte gegen den Glue**: Die Korrektur saß vor einem Kompressor mit Ratio 2 weit über seiner Schwelle,
  der jede Korrektur halbierte (+4 dB brachten +1,9 LU). Jetzt Glue parallel zu 35 % (8.4), die Korrektur dahinter.
- **Die Ziele des Levelers sind gemessen**: `analyze_ref.py` misst jetzt das lauteste 20-s-Fenster (`loud20`), das, was
  der Leveler misst. Hypnotic −10, Ostgut −9,5, Dub −11,5, Raw −9,5 LUFS statt der Schätzung "integriert + 1 dB".
- **Der Multiband-Duck nahm die Hälfte**: Eine subtraktive Weiche (x minus Tiefpass) summiert zwar exakt, ist aber
  nicht phasengleich; bei 60 Hz kamen 5,5 statt 10 dB an. Jetzt eine Linkwitz-Riley-Weiche mit Allpass im Tiefband:
  −9,9 / −2,9 / 0,0 dB, in Ruhe ein Allpass.
- **Drei Verletzungen der Blockgrößen-Regel**, gefunden von einem neuen Test mit allen Stimmen zugleich: die
  Filterfahrt der Drone (Beat aus dem Spannenanfang fortgezählt; jetzt auf dem Raster der Engine gesetzt), Akkord- und
  Ping-Stimmen, die erst am Spannenende frei wurden (ihre Filter liefen bis dahin weiter; jetzt frei am Sample).
- **Diodenleiter, Polivoks und Wasp schwingen unter schneller Hüllkurve auf das 27-fache** (Ephemeris prüft nur bei
  festem Cutoff): parametrisches Pumpen der Integratoren. Ein weiches Knie am Filterausgang (unter 1 unberührt, bei 2×)
  und die Grenze des Cutoffs bei 0,42 der Rate wie in Ephemeris.
- **Die hellere Balance macht die Studie unruhiger**: Hats und Ping wiegen im Onset-Profil 10 dB mehr, Hypnotic fiel auf
  0,72. `reroll` für Hypnotic neu kalibriert: 0,15 trifft 0,87 (0,84 bis 0,87 über drei Seeds); der Unterschied zu
  Ostgut sind jetzt die Zyklen des Pings.
- **Ruhende Stimmen kosteten die Hälfte der Rechenzeit**: Bass, 303, Drone und Textur rechneten auch schweigend. Jetzt
  ruhen sie ab dem Sample, an dem ihre Hüllkurve endet (11- auf 20-fache Echtzeit).

Abweichungen vom Plan, bewusst:
- **Die Granularwolke ist neu geschrieben**, nicht aus Noctuary kopiert: Dessen `GrainCloud` bringt die Aetherizer-
  Hälfte mit (Rückkopplungsspirale durch einen FFT-Shifter, Resonatoren auf der Skala, Hawkes-Schwarm) und braucht das
  Stimmungssystem. Eine Reduktion braucht die schlichte Wolke (`fx/Cloud.h`, ein Zehntel der Zeilen).
- **Der Rumble bleibt mono.** Die Breite kommt aus Raum, Echo, Flächen und Ping, wie die Stems zeigten.
- **Unter 120 Hz strikt mono** (Seite −41 bis −64 dB), die Referenzen haben dort −23 bis −27 dB Seite: nicht
  nachgeahmt (8.2).

Offen, für Phase 4: die Klangrezepte der Stile (Hypnotic ist zu breit und hat 4 dB zu viel bei 250 Hz–1 kHz; Dub ist
über 5 kHz 7 dB zu hell und bei 1–5 kHz 6 dB zu dunkel; Ostgut und Raw sind 3 bis 4 dB zu schmal), der Lautheitsverlauf
(LRA 1 bis 3 statt 1,3 bis 4,7: die Form ist noch flach), Stems, deren Summe die Mischung exakt ergibt (die geduckten
Rückwege teilen sich einen Ducker), und der Hörvergleich je Profil durch den Nutzer.

**27.09.2026, abends: Referenzmessung und der größte Teil von Phase 2.** Die 30 Referenztitel (`Tools/ref_sets.txt`)
liegen als Tonspur in `%TEMP%\totality_refs` (Entscheidung 16.1: Audio behalten, außerhalb des Repos) und sind mit
`Tools/analyze_ref.py` vermessen; im Repo stehen nur die Statistiken (`Tools/ref_stats.json`). Dasselbe Werkzeug misst
Totalitys Renders.

| Größe (Median) | Hypnotic (7) | Ostgut (11) | Raw (6) | Dub (6) | alle (30) | Studie, Hypnotic (Seed 7/8) |
|---|---|---|---|---|---|---|
| Tempo, BPM | 128,0 | 126,9 | 133,9 | 124,3 | 128,1 | 130 |
| Kick-Grundton im Ausklang, Hz | 55,0 | 50,3 | 51,1 | 58,4 | 54,1 | 55,1 |
| Sub-Anteil (20–60 von 20–250 Hz) | 0,45 | 0,51 | 0,51 | 0,40 | 0,49 | 0,39–0,41 |
| Anteil über 5 kHz | 0,5 % | 1,6 % | 0,7 % | 0,1 % | 1,2 % | 0,5 % |
| Schwerpunkt (Leistung), Hz | 252 | 331 | 170 | 182 | 248 | 140 |
| Seite zu Mitte über 200 Hz, dB | −4,5 | −6,2 | −8,1 | −7,9 | −7,2 | −27 |
| Lautheit integriert, LUFS / LRA, LU | −11,5 / 4,7 | −10,8 / 3,0 | −9,9 / 1,3 | −12,2 / 3,4 | −11,2 / 3,0 | −14,5 / 0,7 |
| Takt-Ähnlichkeit (ohne Schwelle, beste Distanz 1/2/4 Takte) | 0,87 | 0,955 | 0,92 | 0,93 | 0,95 | 0,87 |
| Mikroveränderung, dB je Takt und Band | 0,53 | 0,43 | 0,37 | 0,69 | 0,51 | 0,48 |
| Reduktionen je Titel / Länge in Takten | 1,6 / 3 | 2,0 / 1,5 | 0,5 / 3 | 0,3 / 5 | 1,3 / 2 | 1 / 8 |
| Formgrenzen im Abstand von 8 Takten (Zufall ≈ 0,38) | 0,57 | 0,57 | 0,50 | 0,57 | 0,57 | – |

Befunde der Messung:
- **Das Werkzeug trifft das Beatport-Tempo bei 16 von 18 Titeln auf 0,5 BPM**; die zwei Abweichler (*Immolare* −3,
  *The Dancer* +1,9 BPM) sind vermutlich andere Fassungen auf YouTube. Die erste Fassung legte die Kick auf das 16tel
  vor dem Schlag (Ausrichtung am Energiemaximum statt am Einsatz) und rasterte den Kick-Grundton auf 5,4 Hz; beides am
  eigenen Render gefunden und behoben.
- **Die Spektralziele von Dok. 8.7 sind so nicht reproduzierbar**: Leistungsgewichtet liegen über 5 kHz 1,2 % und der
  Schwerpunkt bei 248 Hz, nicht bei 7 % und 1,5 bis 3,5 kHz (vermutlich betragsgewichtet oder anders gefiltert gemessen).
  Kalibriert wird gegen die eigene Messung.
- **Der Kick-Grundton liegt bei 54 Hz**, nicht bei den 60 Hz des Korpus in Dok. 8.7; Totalitys Standard (tonal auf 41 bis
  62 Hz, A → 55 Hz) passt.
- **Reduktionen sind kurz**: im Median 2 Takte, 1,3 je Titel; lange Kick-outs sind die Ausnahme (Efdemin 32, Function
  20, Kobosil 14 Takte). Die Studie mit ihrem 8-Takt-Kick-out liegt am langen Ende. Für die Form (Phase 4) heißt das:
  kurze Schnitte (1 bis 4 Takte) im Tool, lange nur im Peak-Typ.
- **Hypnose, und ein Irrweg dabei.** Die binären Maße (Entropierate, PIR, Positionsentropie, gleiche Takte) ließen die
  Studie viel unruhiger aussehen als die Referenzen; daraufhin wurden Rolling-Hat und Ghost-Kicks zu Loops. Eine
  Gegenprobe zeigte, dass diese Maße vor allem die Binarisierungsschwelle messen: Ein Render aus nichts als Kick,
  Offbeat-Hat und Rumble erreicht schon die Werte der Referenzen, und leise 16tel kippen um die Schwelle. Das
  schwellenfreie Maß (Korrelation der kontinuierlichen Onset-Profile zwischen Takten) zeigt das Gegenteil: Die
  Hypnotic-Titel sind die unruhigste Schule (0,87), Ostgut die gleichförmigste (0,955); Dok. 8.2s Motion, jeden Takt neu
  gewürfelt, trifft Hypnotic genau (0,87), als Loop lag die Studie bei 0,98. Die Loops sind zurückgenommen; die
  Unruhe ist jetzt eine Stilgröße (`RackPlan::reroll`: Hypnotic 1,0, Raw 0,3, Dub 0,25, Ostgut 0,15; gemessen 0,87 /
  0,93 / 0,94 / 0,97). Die binären Maße bleiben im Werkzeug, als schwellenabhängig markiert.
- **Offen gegen die Referenzen**: Breite (−27 dB statt −4,5) und Lautheitsverlauf (LRA 0,7 statt 4,7) fehlen, weil
  Akkorde, Flächen und Dub-Kette (Phase 3) und die Dynamik der Form (Phase 4) fehlen; die Lautheit selbst (−14,5 statt
  −11,5 LUFS) setzt der Leveler (Phase 3), nicht ein höherer Master-Pegel (+3 dB brachten nur +1,6 LUFS, weil Clipper
  und Limiter die Kick-Spitzen nehmen).

Gebaut in Phase 2 (aus Abschnitt 14): Polymeter (3, 5, 6, 7, 12, Reset alle 16 Takte) und Slipping (15, 17, Reset am
Block) als zyklische Layer, Euklid E(3,8)/E(5,16)/E(7,16) neben den Vierteln, Displacement, Ghost-Ketten (Erg. 7),
Trig-Conditions (OH 2:4, Shaker 3:4), Fills (Tom/Conga nach dem letzten Viertel einer 8-Takt-Phrase), die Ping-Stimme
(FM mit √2, Low-Pass-Gate mit Vactrol-Abfall nach Parker und D'Angelo, wandernder Bandpass) samt zyklischer Figur,
Mininotation (`tot_render --patterns`), die symbolische Wiederholung je Stimme (`--stats`), die globalen LFOs (7, 11,
13 Beats, 0,065 und 0,05 Hz) auf Filter, Pegel und Decay der Lanes und den Rumble-Drive, ein Raum-Send (FDN, Rückweg
250 Hz bis 6 kHz) für Hats, Perkussion und Ping. Kalibriert: Rumble-Sub 0 dB (Sub-Anteil 0,45 wie Hypnotic), Rumble-
Ausgang neu (Level weiter "Rumble gegen Kick"), Raum 17 dB unter den trockenen Hats. `ctest` 18 von 18. Aus Phase 2
offen: die Onset-Profile gegen die Referenzen im Einzelnen (die Hypnotic-Titel haben einen fast gleichmäßigen
16tel-Teppich im Höhenband, die Studie ein Offbeat-Profil).

**27.09.2026: Phase 0 fertig, Phase 1 hörbar** (Freigabe des Nutzers: "Ja, bitte ziehe das erst mal so durch", damit
auch die Vorschläge 16.2). `tot_render --seed 7 --minutes 7` spielt die Studie (`compose/Study.h`): sieben Blöcke zu
32 Takten, Kick und Rumble ab Takt 1, Offbeat-Hat ab Takt 9, Rolling-Hat zur Hälfte ab Takt 17, der Hat-Bus öffnet von
1,5 kHz über 24 Takte, dann je Block genau eine Operation (Ghost-Kicks, Open-Hat, die zweite Hälfte der Rolling-Hat,
Shaker/Tom/Rim/Ride gemischt), die Reduktion (Kick-out Takte 16 bis 23 des Blocks bei 55 % des Körpers, Rausch-Swell,
ein Takt Stille, Rückkehr), das Outro subtraktiv. `--low sub` lässt den Sub-Bass statt des Rumble-Sinus das Tief
besitzen und legt eine 16tel-Bassfigur aus dem Bass-Alphabet darunter.

| Prüfstein | Ergebnis |
|---|---|
| Build | Visual Studio 2026, Release, AVX2; ohne Warnungen (/W4) |
| `ctest` | 17 von 17: Selbsttest (14 Abschnitte), Vektortest AVX2, NEON-Shim, skalar |
| Kick: asymptotische Phase (Kick.h) gegen die Messung im Ausklang | 0,24° / 0,27° / 0,29° für Sweep, Resonator, 909, auch bei 0,37 Samples Versatz |
| Rumble-Sub gegen den Kick-Ausklang (Rumble.h), je Schlag 150 bis 300 ms nach der Kick | 0,2° bis 1,3° bei f0 46, 55, 61 Hz und Hall 1 und 3,5 s |
| Unter 80 Hz: Kick + Rumble gegen Kick allein, je Schlag | +0,5 bis +1,3 dB, nie darunter |
| Rumble-Pegel bei Level −9 dB (RMS über einen Schlag gegen die Kick) | −9,1 dB |
| Sub-Bass mit Kick-Lock, 10 bis 100 ms nach dem Einsatz | Korrelation 0,994 mit dem Kick-Ausklang |
| Kit-Pegel, voller Schlag gegen die Kick-Spitze (Dok. 8.7) | alle zwölf Lanes auf 1,5 dB am Ziel |
| Kit-Lanes AVX2/NEON gegen skalar | bitgleich, 768 000 Samples |
| Blockgrößen 1 / 37 / 512 | bitgleich über 20 s der Studie |
| Rack-Regeln | Viertel nur Kick/Clap/Rim, ein Hat je Step, Anker gleich, Motion neu je Takt, Loop-Takt 3 = Takt 1, MPC-Swing exakt, Takt allein = Takt in Folge |
| Master | True Peak −1,47 dBTP (Decke −1), unter 80 Hz Seite > 110 dB unter Mitte |
| Render 7 min | 18 s, 23-fache Echtzeit, 4,4 % eines Kerns; −14,5 LUFS, Crest 13,7 dB |

Gebaut: aus Ephemeris (d047d79) `Vec`, `Dsp`, `Adaa`, `Halfband`, `Clock`, `WavWriter`, `Loudness`, `fx/Dynamics`,
`fx/Reverb`, der Parameter-Store, Partitur-Kurven, MIDI-Kodierer und das Testgerüst; aus Phosphene (76f7100) die Kick,
das Kit mit `PercKernel` und der Ducker; aus Noctuary (7a48fdd) der Oversampler. Neu: die Parametertabellen, die
909-Engine und die Top-Schicht der Kick, `Kick::asymptoticPhase`, der frequenzgeteilte Rumble, der Sub-Bass mit
Kick-Lock, die im Code erzeugte 909-Metalltabelle (60 Teiltöne, 6 Bit), Einzelausgänge der Lanes, das Pattern-Rack mit
allen Matrizen, Velocity- und Timing-Werten von Dok. 8.2/8.3, die Studie, die Engine, `tot_render`.

Befunde unterwegs, alle von Tests oder Messungen gefunden:
- **Der Rumble lag 29 dB unter der Kick** und erreichte den Clip nie (Hallrückweg mit Spitzen bei −13 dBFS): Drive
  tat nichts. Jetzt +12 dB vor dem Clip und +7,5 dB nach den Filtern, beide Konstanten in `testRumbleLevel` gehalten.
- **Die Hats lagen 21 dB zu tief** (Spitze gegen Kick): die Lane-Pegel aus Phosphenes Tabelle passten nicht zu den
  neuen Quellen. Kalibriert auf die Referenzpegel von Dok. 8.7, gehalten von `testKitLevels`.
- **Die erste Phasenprüfung des Rumbles maß die falsche Größe**: Sie korrelierte zwei phasengleiche Sinusse mit
  gegenläufigen Hüllkurven (die Kick fällt, der Sub steigt) und fand 0,26. Jetzt die Phase beider bei f0.

Abweichungen vom Plan, bewusst:
- **Die Kick sättigt mit ADAA erster Ordnung wie in Phosphene, nicht 4× überabgetastet** (5.1): Rumble und Sub setzen
  die Phase der Kick in geschlossener Form fort, und dafür muss `Kick::chainPhase` die Kette exakt kennen; ein
  Halbband-Paar brächte 29,5 Samples Latenz und eine weitere Phasenkurve. Ein Sinus von 45 bis 300 Hz durch tanh aliast
  bei 48 kHz kaum. 4× laufen der Rumble-Clip und der Master-Clipper.
- **Mehr als Phase 1 vorsah**: das ganze Kit und das Rack mit allen Matrizen von Dok. 8.2 samt Regeln und Timing.
  Aus Phase 2 bleiben Ghost-Ketten, Polymeter, Euklid, Displacement, Slipping, Trig-Conditions, Fills, Mininotation
  und die Ping-Stimme.
- **Eine feste Studie statt des Komponisten**, wie bei Ephemeris.

Noch offen in Phase 1 (Stand dieses Eintrags): die Referenzmessung (erledigt, siehe den Eintrag darüber) und das
Urteil des Nutzers nach dem Hören. Die Spektralbalance der Studie ist erwartbar dunkel (über 5 kHz 0,4 % statt der
7 % des Korpus, Schwerpunkt 140 Hz): Ohne Akkorde, Bass-Synth und Pings fehlt die ganze Mitte; kalibriert wird gegen die
gemessenen Referenzen, nicht gegen Korpuszahlen ganzer Mischungen.

**27.09.2026: Planentwurf.** Grundlage siehe oben; Entscheidungen in Abschnitt 16.

## 0. Kurzfassung

Ein Instrument, das aus einem Seed, einem Stilprofil und einer Set-Dramaturgie Berliner Techno zwischen 125 und 136 BPM
komponiert und in Echtzeit synthetisiert, mit dem Hypnotic-Techno der Linie Dozzy, Parker, Mulero als Mitte: einzelne
Tracks von 6 bis 9 Minuten und ganze DJ-Sets von einer bis acht Stunden, mit Blends über 16 bis 32 Takte, hartem
Bass-Swap an der 32-Takt-Grenze und langsamer Tempodrift. Standalone und VST3 für Windows (JUCE), nativ auf der Quest 2,
MIDI-Export aller Layer und Automationen, Stems und DJ-Loops, Offline-Render als Determinismus-Orakel.

Die drei Entscheidungen, die alles andere bestimmen:

1. **Der Loop ist das Stück; Veränderung ist Subtraktion und Mikroereignis.** Psytrance schreibt Motive, Berlin School
   spielt einen Sequenzer-Prozess. Berliner Techno schichtet Loops und nimmt sie wieder weg (Dok. 5: "all energy is
   directed by muting and unmuting looping patterns"). Der Kern ist deshalb ein **Pattern-Rack**: Layer mit
   16-Step-Matrizen aus Onset-Wahrscheinlichkeiten, getrennt in **Anker** (unverändert innerhalb eines Blocks) und
   **Motion** (jeden Takt neu gewürfelt), dazu Polymeter und gleitende Loops (Slipping). Der Komponist arbeitet in
   32-Takt-Blöcken mit **genau einer Operation je Block** (Treppe: Add, Remove, Swap, Hold) und setzt darunter
   Mikroereignisse und Automationsrampen. Die Partitur enthält trotzdem jede Note (Determinismus, MIDI-Export).
2. **Das Tieffundament ist ein gekoppeltes System.** Kick, Rumble und Sub teilen sich das Band unter 150 Hz, und genau
   dort entscheidet sich, ob ein Track schiebt oder sich selbst auslöscht (Erg. 4). Totality baut die Kick mit
   geschlossener Phase (Phosphene), führt den Rumble **frequenzgeteilt**: Hall nur auf 80 bis 300 Hz, darunter ein Sinus,
   der die Phase der Kick an der Übergabe fortsetzt, und duckt ereignisgesteuert vom Kick-Trigger, nicht vom Pegel. Ein
   Track hat einen **Tiefenbesitzer**: Rumble oder Sub-Bass, nie beide voll. Erster Prüfstein ist, was bei Phosphene der
   rollende Bass war: **ein Groove, der rollt** (Abschnitt 14).
3. **Hypnose ist messbar und wird gesteuert.** Hohe Vorhersagbarkeit des Rasters, darüber ein stetiger, kleiner Strom
   von Neuigkeit (Dok. 1: "a hi-hat opening by 2 % can qualify as a major emotional event"). Totality misst beides auf der
   Partitur und auf dem Audio: Entropierate und Predictive Information Rate (Abdallah und Plumbley 2009) der
   Onset-Folgen, dazu die Rate der Klangfarbenänderung von Takt zu Takt. Die Korridore werden an den Referenzen gemessen
   (13.4), und der Komponist wählt je Block unter mehreren Kandidaten den, der im Korridor seines Profils liegt (7.9).

Reihenfolge der Arbeit: zuerst Kick, Rumble, Offbeat-Hat und Sub mit Ducking, bis der Loop rollt; dann die ganze
Pattern-Ebene mit Polymetrie, Slipping und Ghost-Logik; dann Klang (Ping, Bass, Dub-Kette, Mix); dann Form und Set.

## 1. Ziel, Rahmen, Nicht-Ziele

**Ziel.** Auf Knopfdruck ein Track oder Set, das ein Kenner des Ostgut-Ton-, Hypnotic- und Dub-Kanons als stilistisch
glaubwürdig hört und ein DJ auflegen kann: tragende Kick, rollendes Tief, reduzierte Mitten, Raum und Delay als
Instrumente, Form aus 32-Takt-Plateaus mit Mute und Unmute statt Build-Drop, Intro und Outro zum Mixen. Jede Einheit
einzeln sperrbar und neu würfelbar. Alles reproduzierbar aus Seed, Profil und Sperren.

**Rahmen** (wie Phosphene und Ephemeris).
- Plattformen: Windows x64 (AVX2), Quest 2 (arm64, NEON). Linux als Nebenprodukt des frameworkfreien Kerns.
- Sample-Rate 44,1/48/96 kHz, Blöcke 1 bis 2048; Quest 48 kHz, 256er Blöcke (Oboe).
- Kern ohne Framework, C++20, keine Allokation im Audio-Thread, kein Fast-Math; der Offline-Render ist das Orakel;
  Blockgrößen 1, 37 und 512 bitgleich; ein Takt allein gerendert gleich demselben Takt in der Folge.

**Nicht-Ziele.**
- Kein Klon einzelner Künstler oder Tracks. Profile tragen beschreibende Namen; Künstler stehen nur in der
  Dokumentation als Hörreferenz.
- Kein Hard Techno über 140 BPM, keine EDM-Drops, keine Riser-Kaskaden, keine Snare-Rolls (Dok. 8.2, 8.5). Ein
  Rausch-Swell über 8 bis 16 Takte ist das Äußerste.
- Keine Vocals, keine melodischen Leads im Pop-Sinn.
- **Keine Samples** als Vorschlag (16.2): auch die 909-Hats entstehen aus einer im Code erzeugten Metalltabelle.
- Keine neuronale Audio-Erzeugung im Kern: über Stunden weder steuerbar noch deterministisch, auf der Quest zu teuer.
- Kein Cloud-Modell in der Echtzeitschleife.

## 2. Was Hypnotic Berlin Techno ausmacht (die musikalische Spezifikation)

Das Recherchedokument ist die Spezifikation. Dieser Abschnitt fasst zusammen, was der Generator daraus macht, und
verweist für Zahlen und Belege auf Dok. Die Zahlen sind Hypothesen, bis sie an Referenzen gemessen sind (13.4);
gespeichert werden nur Statistiken, nie Audio.

### 2.1 Drei Linien und eine Schule (Dok. 1)

| Linie | Kern | Beispiele (Dok. 1, Tabelle) | Profil |
|---|---|---|---|
| Detroit–Berlin über Tresor (ab 1991) | Mills, Hood; reduzierter, funktionaler Loop | Jeff Mills *The Dancer* (131) | Raw/Peak, Hypnotic |
| Dub Techno: Basic Channel, Chain Reaction, Hard Wax (ab 1993) | 4/4, tiefe repetitive Bässe, Delay und Hall auf allem, Mischpult als Instrument | Quadrant *Infinition* (128) | Dub |
| Ostgut-Ton-Generation (Berghain ab 2004) mit Klockworks, MDR, Figure, Dystopian, Sandwell District | "austere, stripped-down techno with rigid kick drums", "four-to-the-floors shrouded in a dubby fog" | Klock & Dettmann *Dawning* (125), Function *Disaffected* (127), Dettmann *Deluge* (130), Rødhåd *Haumea* (130) | Ostgut |
| **Die Hypnotic-Schule** (Dozzy, Mulero, Mike Parker; Mote-Evolver, Pole Group, Semantica) | 125 bis 132 BPM, "trance-inducing repetition", mikroskopische Ereignisse; prozessierte 909-Kicks mit Rumble-Layern, FM- und Modular-Sequenzen mit LFO, bitcrushed Hats | PAS *Kat* (132), Mulero *Generator* (135) | **Hypnotic (Mitte)** |

Zwei Tempolager (Dok. 1): 125 bis 130 (früher Ostgut-, MDR-, Sandwell-Kern) und 131 bis 135 (PAS, DVS1, Mulero,
Kobosil, Mills); Tracklängen von 7 bis 14 Minuten sind im Kanon normal.

### 2.2 Was sie verbindet

1. **PoumTchak ist invariant** (Dok. 2): Kick auf den Vierteln, Offbeat-Hat. Die Kick hält Zeit, statt zu prügeln.
2. **Der Loop ist das Material** (Dettmann: "Techno is a loop for me"). Interesse entsteht aus Verschiebung
   (Displacement-Dissonanz), Polymetrie und Mikroveränderung, nicht aus Motiventwicklung (Dok. 2, Butler, Garcia).
3. **Tonalität statisch** (Dok. 4): Moll oder monoton, ein Akkord oder keiner, Orgelpunkt, eindeutige Tonika im Bass.
4. **Mitten reduziert, Tief gekoppelt** (Dok. 3, 6): Kick plus Rumble oder Sub als einzige Tiefquelle, Mono unter 120
   bis 150 Hz, schmales Stereobild (0,16 bis 0,24).
5. **Raum und Delay sind Instrumente** (Dok. 3): Dub-Delays mit Feedback bis an die Selbstoszillation, lange Hallräume
   auf Akkorden, der Rumble als Hall der Kick.
6. **Form aus 32-Takt-Plateaus** (Dok. 5): genau eine Änderung je Block, Reduktion statt Breakdown, lange Intros und
   Outros für den DJ, höchstens zwei Höhepunkte.
7. **Lange Sets** (Dok. 1, 6): 6 bis 14 Stunden, Live-Rekomposition auf mehreren Decks, Tempo monoton, Blends über 16
   bis 64 Takte.
8. **Die Hand am Mischpult** (Dok. 3, 6): Sends, Filter und Delay-Würfe von Hand gefahren, über 16 bis 32 Takte.

### 2.3 Tempo und Raster

- 16 Steps je Takt; bei 130 BPM Beat 461,5 ms, Achtel 230,8 ms, 16tel 115,4 ms, Takt 1,846 s (Dok. 2).
- Global 125 bis 136 BPM; Profilbereiche in 2.8. Dok. 8.1 setzt 128 bis 136 als Generatorziel mit Standard 130; die
  Ostgut- und Dub-Referenzen liegen bei 125 bis 128 (Dok. 1), deshalb reichen diese beiden Profile bis 125.
- 4/4 als Rahmen; Polymeter mit Perioden 3, 5, 6, 7, 12 Steps (Dok. 8.2) und gleitende Loops mit 15 oder 17 Steps
  (Erg. 5, 6.4).
- Swing nach Linn-Definition (Verzögerung jedes geraden 16tels), 50 bis 58 %, Standard 53 %, nur auf Hats, Shaker,
  Percussion; Kick und Haupt-Clap immer gerade (Dok. 2, 8.3).

### 2.4 Der Groove

Die Pattern-Ebene übernimmt die Onset-Matrizen von Dok. 8.2 (Kick, Ghost-Kick, Rumble, CH-Offbeat, CH-16tel, OH, Ride,
Clap A/B, Clap-Ghost, Shaker, Tom/Conga, Rim, Bass, Stab) samt ihren Regeln (Lücken-Reservierung, ein Hat je Step,
Kollisions-Dip, Loop-Länge, Trig-Conditions, Polymeter, Euklid, Displacement, Resyncopation, Fill, Dichte-Deckel) und die
Velocity- und Timing-Tabelle von Dok. 8.3. Neu gegenüber dem Dokument: bedingte Ghost-Wahrscheinlichkeiten (Erg. 7),
gleitende Loops (Erg. 5), die Velocity als Klangfarbe (Brootle, Dok. 2) bei jeder Stimme. Einzelheiten in Abschnitt 6.

### 2.5 Klang

Kick als Zweischicht-Stimme (909-Topologie, 808-Resonator, Sinus-Rezept), Rumble als Hall der Kick, Sub-Sinus und
Bass-Synth (SH-101/303-Idiom), 909/808-Hats und Becken, 909-Clap, tiefe Snare, Toms, Rim, Shaker, der Dub-Chord mit
seiner Effektkette, Texturen (Vinylknistern, Netzbrummen, Rausch-Erosion) und frei laufende, inkommensurable LFOs
(Dok. 3, 8.4). Neu: die **Ping-Stimme** (FM mit inharmonischem Verhältnis oder angeschlagener Low-Pass-Gate, Erg. 1),
die in der Hypnotic-Mitte die Rolle übernimmt, die in Dub und Ostgut der Akkord hat. Einzelheiten in Abschnitt 5.

### 2.6 Harmonik (Dok. 4, 8.6)

Moll 0,88, monoton 0,08, Dur 0,04; Äolisch 0,6, Dorisch 0,15, äolisch-dorischer Hexachord 0,1, Phrygisch/b2 0,1,
Moll-Pentatonik 0,05, harmonisch Moll 0. Ein Akkord je Track (p 0,8): i, i7, i add9, i mit 4 im Bass; ein zweiter nur
als Shuttle (bII, bVII, iv, bIII). Keine V-i-Kadenz, kein Leitton, keine maj7. Bass-Alphabet aus ein bis zwei
Tonhöhenklassen; Motivänderung nur alle 4/8/16 Takte. Unter 150 Hz nur Unisono, Oktave, Quinte. Tonart-Wechsel
p 0,04, nur an einer 32-Takt-Grenze nach einem Übergang ohne Tonales.

### 2.7 Form (Dok. 5, 8.5)

Track = Intro Body Outro, 192 bis 256 Takte, 6 bis 8 Blöcke zu 32. Body aus 4 bis 6 Blöcken mit je einer Operation;
Reduktion als KickOut über 4, 8 oder 16 Takte mit optionalem Swell und Rückkehr; Ereignisse alle 8 oder 16 Takte
(Mute-One-Hit, One-Bar-Dropout, Delay-Throw, seltenes Fill); Automation auf drei Zeitskalen (Mikro 4 bis 8 Takte, Meso
16 bis 32 Takte blockbündig, Makro über den ganzen Track). Neu: der dritte Formtyp **Endless** (Erg. 8), 7.2.

### 2.8 Vier Stilprofile, Hypnotic in der Mitte

Ein Stilprofil ist, wie in Phosphene und Ephemeris, ein Vektor von Gewichten und Bereichen: Tempo, Formtypen,
Tiefenbesitzer, Kick-Rezept, Layer-Wahrscheinlichkeiten, Polymeter- und Slipping-Anteil, Swing, Dichte-Deckel,
Ereignisrate, Hypnose-Korridor, Klangrezepte, Dub-Anteil, Lautheitsziel. Zwischen Profilen wird interpoliert; zwei
Achsen aus Dok. 8.0 (Dub-Anteil 0 bis 1, Hypnotic-Anteil 0 bis 1) sind zusätzlich direkt als Regler zugänglich.

| Profil | Tempo | Tief und Kick | Rhythmus | Klang | Formtypen | Hörreferenz |
|---|---|---|---|---|---|---|
| **Hypnotic** (Standard) | 128 bis 133 | Rumble-Besitz (p 0,7); prozessierte 909-Kick, rollend | 2 bis 3 Polymeter-/Slipping-Lagen, Rolling-Hat, Ghost-Kicks, wenig Clap | Ping-Ostinati (5 oder 7 Steps), Modular-Sequenzen mit LFO, Drones, Granulartextur, bitreduzierte Hats; Akkord selten | Tool 0,5, Endless 0,3, Peak 0,2 | Donato Dozzy, Voices From The Lake, Mike Parker, Oscar Mulero, Rrose (Labels Prologue, Semantica, Pole Group, Mote-Evolver) |
| **Ostgut** | 125 bis 131 | Zweischicht 909/808; Rumble oder Sub | Clap auf 2 und 4, Ride-Achtel, OH auf dem Offbeat, eine Polymeter-Lage | Dub-Stabs sparsam, Delay-Würfe, 303 selten | Tool 0,6, Peak 0,4 | Klock, Dettmann, Function, Norman Nodge, Rødhåd, Efdemin |
| **Dub** | 125 bis 130 | Sub-Bass-Besitz (p 0,8), weiche Kick | Swing 50 %, Bewegung aus dem Delay (Dok. 8.3), Hats reduziert | Dub-Chord mit voller Kette (Bandecho, Feder, Platte), Vinyl-Knistern, Bitreduktion | Tool 0,7, Endless 0,2, Peak 0,1 | Basic Channel, Chain Reaction (Quadrant, Porter Ricks, Monolake) |
| **Raw/Peak** | 131 bis 136 | Kick härter, Distortion vor dem Tiefpass ("rolling"), Rumble verzerrt | dichter, 16tel-Hats, Toms, Industrial-Percussion, längere Reductions | Übersteuerung, Rauschen, metallische Pings | Peak 0,6, Tool 0,4 | Planetary Assault Systems, DVS1, Kobosil, Jeff Mills |

Tempobereiche und Gewichte sind [I]. Das Hypnotic-Profil wird zuerst kalibriert (Entscheidung 16.1); die Ostgut-, Dub-
und Raw-Referenzen des Dokuments (13.4) messen die anderen drei.

### 2.9 Was Hypnose ist, als Zahl

Dok. 1 und 5 beschreiben denselben Mechanismus aus drei Richtungen: Repetition plus Mikroveränderung (Dettmann,
Goldmann, Garcia), der Groove als Tasche, in der man bleibt (Hood: "Stay in the pocket and watch it put people in a
trance", höchstens vier Elemente zugleich), und die Winzigkeit der Ereignisse (2 % Hat-Öffnung). Totality macht daraus drei
Größen, die gemessen und gesteuert werden:

- **Vorhersagbarkeit des Rasters:** Entropierate h der Onset-Folge je Band oder Layer (Zustand je 16tel), geschätzt als
  Markov-Kette über ein 32-Takt-Fenster.
- **Neuigkeitsstrom:** Predictive Information Rate b nach Abdallah und Plumbley (2009), für eine Markov-Kette mit
  Übergangsmatrix a geschlossen b = H(a²) − H(a). Die Autoren vermuten in ihr die Erklärung des umgekehrten U zwischen
  Zufälligkeit und ästhetischem Urteil: zu wenig ist Trivialität, zu viel Nervosität.
- **Mikroveränderungsrate:** der mittlere Abstand der Klangfarbe (Bandenergien, Schwerpunkt, Hat-Decay) von einem Takt
  zum nächsten, ohne die Block-Operationen.

Die Zahlen der Ergänzung ("Vorhersagbarkeit > 90 %", "1 bis 5 % Informationszuwachs") stehen nicht in der Arbeit von
Abdallah und Plumbley und werden nicht übernommen; die Korridore kommen aus der Messung (13.4).

**Nachtrag 27.09.2026 (Referenzmessung):** Auf Audio gemessen hängen Entropierate und PIR stark an der Schwelle, mit
der Onsets binarisiert werden; schon ein reiner Anker-Loop erreicht die Werte der Referenzen. Das tragfähige Maß der
Wiederholung ist die schwellenfreie Takt-Ähnlichkeit (Korrelation der Onset-Profile zwischen einem Takt und dem 1, 2
oder 4 Takte davor): Hypnotic 0,87, Raw 0,92, Dub 0,93, Ostgut 0,955. Zusammen mit der Mikroveränderung (0,37 bis
0,69 dB je Takt) bildet sie den Korridor. Auf der Partitur (7.9) gilt dieselbe Größe symbolisch.

### 2.10 Stand der Technik (SOTA)

Dok. 7 ist die Übersicht und wird hier nicht wiederholt. Die Kurzfassung: Es gibt keinen validierten Techno-Generator.
Die dokumentierten regelbasierten Systeme (Eigenfeldts GERP, GEDMAS, GESMI; Collins' Infno) arbeiten mit
Onset-Wahrscheinlichkeitsmatrizen, Sektionsalphabeten und Markov-Ketten erster Ordnung und berichten keinen formalen
Hörtest; Produzentenformalismen (Elektron-Trig-Conditions, Ableton-Groove, Tidal-Mininotation) sind direkt
wiederverwendbar. Totality folgt diesem Weg: Regeln und Wahrscheinlichkeiten mit Zustand, kalibriert an Messungen, keine
lernenden Anteile zu Beginn.

Eigener Stand, der hier zählt: Phosphene (Kick mit geschlossener Phase und Phasenkopplung an den Bass, zwölf
Percussion-Lanes in Registern, ereignisgesteuertes Ducking, Form mit Energiebogen und DJ-Überlappung, die
`ref_*.py`-Messwerkzeuge), Ephemeris (Schaltungsfilter, VCO-Modelle, Modulationsmatrix, Gesten-Engine, Bandecho, Feder,
Platte, Leveler, Klänge als Programmwechsel, Release-Pipeline) und Noctuary (Faltung mit 1000 erzeugten
Impulsantworten, 4×-Oversampling, Mid/Side, Patina, spektrales Ducking, Granularwolke, Lautheit mit True Peak).

### 2.11 Die Ergänzung, geprüft

| Punkt | Urteil | Was Totality daraus macht | Beleg und Vorbehalt |
|---|---|---|---|
| **Erg. 1** FM-Ping, Low-Pass-Gate | übernommen | eigene Stimme `Ping` (5.5): FM mit inharmonischem Verhältnis oder angeschlagenes LPG mit Vactrol-Verhalten, Ostinati über 5 oder 7 Steps | LPG-Modell nach Parker und D'Angelo (DAFx 2013); Verhältnisse und Bereiche [I] |
| **Erg. 2** Berghain-Akustik, Fletcher-Munson | teilweise | kein fester "Dark Tilt"; das Master-Tilt zielt auf die gemessene Spektralverteilung der Referenzen (Dok. 8.7: > 5 kHz ≈ 7 %, Schwerpunkt 1,5 bis 3,5 kHz) | 18 m Deckenhöhe in der Presse belegt; **RT60 3 bis 4 s und "ungedämpfte Betonflächen" nicht belegt**, für die Halle am Berghain ist eine raumakustische Beratung dokumentiert (K5 Akustik), ohne Zahlen. Das Gebäude ist ein ehemaliges Heizkraftwerk. Richtung der Isophonen (ISO 226) richtig, die Schlussfolgerung auf eine feste EQ-Kurve nicht zwingend |
| **Erg. 3** Dubplates & Mastering, Schneidephysik | übernommen als Option | Master-Profil **Cut** (8.6): Mono unter einer Weiche, dynamische Höhenbegrenzung 6 bis 10 kHz, Tiefpass 16 kHz, keine Gegenphase 80 bis 300 Hz | Neumann VMS 70 bei D&M belegt (PS Audio; Lathe Trolls). Vertikalauslenkung durch L−R und Erhitzung des Schneidkopfs durch Höhen sind Schneidepraxis; Becker selbst relativiert den Mono-Bass-Mythos (Dok. 6), deshalb Option, nicht Standard |
| **Erg. 4** Phasenkohärenz Kick und Rumble | übernommen, Kern von 5.2 | Frequenzteilung vor dem Hall, Sinus unter 80 Hz setzt die Phase der Kick fort; Selbsttest misst die Summe | deckt sich mit Phosphenes Befund (Bass-Sub getrennt, weil das Filter die Grundtonphase um 34° drehte); 80 Hz ist Startwert [I] |
| **Erg. 5** Slipping Loops | übernommen, mit Korrektur | Layer mit 15 oder 17 Steps (6.4), 31 und 33 als langsame Variante | Rechnung korrigiert: kgV(15, 16) = 240 Steps = **15 Takte**, kgV(17, 16) = 272 = **17 Takte** (nicht "15 bzw. 16"). Reichs *Piano Phase* ist stetige Tempodifferenz; der diskrete 16tel-Versatz entspricht eher *Clapping Music* |
| **Erg. 6** Predictive Information Rate | übernommen als Messgröße, ohne die Zahlen | Hypnose-Korridor (2.9, 7.9, 13.5) | Abdallah und Plumbley, Connection Science 21(2), 2009, 89 bis 117, belegt; die Prozentwerte der Ergänzung nicht |
| **Erg. 7** Markov-Ghosts | übernommen | bedingte Faktoren je Ghost-Layer auf die Wahrscheinlichkeiten von Dok. 8.2 (6.3) | passt zu GEDMAS (Markov erster Ordnung, Dok. 7); Werte [I], ohne Stems nicht direkt messbar |
| **Erg. 8** Endless Groove | übernommen | dritter Formtyp `Endless` (7.2) | im Dokument schon angelegt ("Tool-Variante: voller Loop ab Takt 1", p 0,15; "No One Around ... just rolls", Dok. 5, 8.5); die Label-Zuordnung der Ergänzung ist nicht belegt |

## 3. Architektur

```
 Stilprofil(e) + Seed + Set-Dramaturgie + Sperren
            │
            ▼
   ┌──────────────────────────────┐  Partitur: Noten je Layer, Automations-    ┌───────────────────────────────────┐
   │ Composer-Thread              │  kurven, Block-Ops, Mikroereignisse,       │ Audio-Thread                      │
   │ Set → Track → Block (32) →   │  Marken, Cues; 16+ Takte voraus            │ Deck A │ Deck B │ (Deck C)         │
   │ Takt → Step                  │ ───────── lock-free Queue ───────────────▶ │  je Deck: Kick, Rumble, Sub/Bass, │
   │ Pattern-Rack, Harmonie,      │                                            │  Kit, Ping, Chord, Drone, Textur  │
   │ Kandidaten + Hypnose-Korridor│ ◀── Position, Live-Eingriffe (Mute,        │  → Kanalzüge → Dub-Sends → Bus    │
   └──────────────────────────────┘     Throw, Kill, Filter) ───────────────── │ DJ-Mixer: Isolator, FX-Send       │
            │                                                                   │ → Master: Glue, Clip, TP-Limiter  │
            ▼                                                                   └───────────────────────────────────┘
   MIDI (SMF 1), Stems, DJ-Loops, Cue-Marken, .totset, OSC                     Offline-Render (Orakel), Leveler
```

**Der Unterschied zu den Geschwistern.** Phosphene schreibt Motive in Sektionen, Ephemeris lässt ein Sequenzer-Rack
voraus laufen. Totality lässt ein **Pattern-Rack** voraus laufen: Layer mit Matrizen, Loop-Länge, Periode, Rotation,
Displacement, Trig-Conditions und dem Zustand ihrer Ghost-Ketten. Der Komponist schreibt zweierlei in die Partitur:
Noten (für Audio und MIDI) und **Block-Operationen** (Add, Remove, Swap, Hold, Reduction), damit Sperren und Neuwürfeln
auf der Ebene von Block und Layer arbeiten. Automationen sind parametrische Kurven wie die Gesten in Ephemeris und dürfen
über das Vorausfenster hinausreichen.

**Decks.** Ein Deck ist ein vollständiges Instrument für einen Track (alle Stimmen, Kanalzüge, Sends, Track-Bus). Ein Set
braucht zwei Decks, während eines Blends beide aktiv; ein drittes Deck trägt die Live-Rekomposition (7.7: ein Loop des
vorigen Tracks läuft unter dem neuen weiter). Der DJ-Mixer summiert die Decks mit Isolator-EQ je Deck und einem
gemeinsamen FX-Send. Außerhalb von Blends rechnet nur ein Deck.

**Schichten im Kern (`Core/`):**
- `tot/Vec.h`, `tot/Dsp.h`, `tot/Adaa.h`, `tot/Halfband.h`, `tot/Oversample.h`: SIMD, Grundbausteine, Oversampling.
- `tot/Clock.h`, `tot/Score.h`, `tot/Params.h`, `tot/Presets.h`, `tot/Midi.h`, `tot/SetFile.h`, `tot/Cue.h`,
  `tot/Loudness.h`, `tot/Leveler.h`.
- `tot/pattern/*`: Rack, Layer, Matrizen, Ghost-Ketten, Polymeter und Slipping, Trig-Conditions, Groove (Swing,
  Timing, Velocity), Mininotation.
- `tot/compose/*`: Komponist, Form, Blöcke und Ereignisse, Automation, Harmonie, Stilprofile, Set, Evaluation.
- `tot/synth/*`: Kick, Rumble, Bass, Kit (PercKernel), Ping, Chord, Drone, Textur, Filter, VCO, Modulation.
- `tot/fx/*`: Bandecho, Feder, Platte, FDN, Faltung, Tempo-Delay, Phaser, Lo-Fi, Sättigung.
- `tot/mix/*`: Kanalzug, Ducker, Multiband-Ducker, Bus, Deck, Isolator, Master.

**Threads und Determinismus** wie Phosphene und Ephemeris: Audio allokiert nie; der Komponist arbeitet in Häppchen mit
der Frist "Queue nie unter 16 Takten"; ein RNG je Modul mit `fork()` aus dem Seed. Die Motion-Würfe eines Takts hängen
an (Track-Seed, Layer, Taktindex) und am Zustand der Ghost-Kette, den der Komponist deterministisch fortschreibt; nie an
der Wanduhr.

## 4. Wiederverwendung

**Modulkopie, kein Link**, wie bisher. Jede kopierte Datei nennt im Kopf Herkunft und Stand. Das Gerüst kommt aus
Ephemeris, weil es die neueste Fassung der gemeinsamen Architektur ist (Unterordner, Presets als Programmwechsel,
Leveler, Test-Gerüst, Release-Pipeline); Schlagzeug, Bass und Messwerkzeuge kommen aus Phosphene, das als einziges
Geschwister trommelgetrieben ist; Räume und Mastering-Bausteine aus Noctuary.

| Modul | Herkunft | Einsatz hier | Anpassung |
|---|---|---|---|
| `Vec.h`, `Dsp.h`, `Adaa.h`, `Halfband.h`, `Clock.h`, `WavWriter.h` | Ephemeris | überall | Namensraum |
| `Params.h`, `Presets.h` (Klänge als Programmwechsel), `Score.h`, `Midi.h`, `SetFile.h`, `Cue.h` | Ephemeris | Parameter, Partitur, Export | Module `kick`, `rumble`, `kit`, `ping`, `bass`, `chord`, `drone`, `dub`, `deck`, `djmix`, `master`; Block-Ops in der Partitur; `.totset`; OSC `/tot/...` |
| `Leveler.h`, `Loudness.h` | Ephemeris | jeder Track auf das Lautheitsziel seines Profils | Ziel −11 bis −10 LUFS statt Ambient-Pegel (8.5) |
| `compose/Composer`, `compose/Style`, `compose/Harmony` | Ephemeris | Gerüst des Composer-Threads, Profile mit designierten Initialisierern, Tonart | Block-Grammatik neu (7.2); Harmonie auf Dok. 8.6 zurückgeschnitten |
| `compose/GestureEngine` (Minimum Jerk, zwei Hände) | Ephemeris | Automationskurven, Delay-Würfe, Filterfahrten | Kurven auf 4/16/32-Takt-Rastern verankert |
| `synth/Filters.h` (Schaltungsfilter: Moog, SEM, Prophet, Juno, Diode, Korg35 …) | Ephemeris | Bass (Diode/303, Moog), Chord (Prophet, Juno), Percussion-Resonanz | keine |
| `synth/ModVoice`, `VoiceKernel`, `Vco`, `Modulation` (LFOs, Matrix) | Ephemeris | Bass-Synth, Chord-Stimmen, Drones, globale LFOs | Lanes für sechs Akkordstimmen |
| `fx/TapeEcho`, `fx/Spring`, `fx/Plate`, `fx/Bbd`, `fx/Reverb`, `fx/Dynamics` | Ephemeris | die Dub-Kette (RE-201 = Bandecho plus Feder), Platte der Akkorde, Master | Delay-Zeiten in Beats; Feedback bis zur Selbstoszillation als Automation |
| `Kick` (geschlossene Phase, Resonator-Engine nach Werner, Phasenziel, Tail-Limit) | Phosphene | Kick-Kern (5.1) | 909-Engine ergänzen; Übergabephase an den Rumble-Sub |
| `Perc`, `PercKernel` (12 Lanes: Rauschen, 808-Metall, Modal, Ton, FM; Auto-Pan) | Phosphene | das Kit (5.4) | Rollen neu (CH-Offbeat, CH-16tel statt Zap/Blip); 909-Metalltabelle als fünfte Quelle; Low-Cut-Regel prüfen (Toms) |
| `Rhythm` (Euklid nach Bjorklund, LHL, Fills, Layer-Reihenfolge) | Phosphene | Pattern-Rack | Onset-Matrizen, Ghost-Ketten, Slipping neu |
| `Ducker` (ereignisgesteuert, sub-sample-genau) | Phosphene | Sidechain von Rumble, Bass, Pads, Hall | Multiband-Variante (8.3) |
| `Bass` (Sub-Sinus getrennt, LR8-Split, Bite, Phasenkopplung an die Kick), `Acid`, `DiodeLadder` | Phosphene | Sub und Bass-Synth (5.3), 303-Linien im Ostgut- und Raw-Profil | Tiefenbesitz statt Rolling-Bass-Muster |
| `TempoDelay`, `PsyFx` (Phaser, Flanger, Frequenzschieber) | Phosphene | Dub-Delays, Phaser der Akkorde, Delay-Mod auf Hats | Stutter und Flanger nicht standardmäßig |
| `Form` (Energiebogen, DJ-Überlappung, 32-Takt-Grenzen), `Quality`, `Rating`, `Probe` | Phosphene | Set-Bogen, Qualitätsstufen, Bewertungen | Energiebogen als Tempo- und Dichtebogen über Stunden |
| `Tools/metrics.py`, `ref_kick.py`, `ref_bass.py`, `ref_perc_profile.py`, `ref_band_balance.py`, `ref_width.py`, `ref_arrange.py`, `ref_slot_profile.py`, `mix_audit.py` | Phosphene | Referenzmessung (13.4) | Tempobereich 120 bis 140; Onset-Folgen für den Hypnose-Korridor |
| `Oversample.h` (4× Halbband um Nichtlinearitäten) | Noctuary | Kick-Shaper, Rumble-Clip, Bus-Sättigung, Master-Clipper | keine |
| `Convolution.h` (partitioniert, bis eine Minute IR) und die 1000 Impulsantworten aus `ImpulseGen` | Noctuary | Rumble-Hall (Desktop), Hallräume der Akkorde | auf dem Quest FDN statt Faltung |
| `Effects.h`: `MidSide`, `Patina`, `Unmask`, `EarlyRoom`, `Diffuser` | Noctuary | Mono-Bass, Band-/Lack-Alterung des Masters, spektrales Ducking der Hallfahnen, Raumeindruck | `Unmask` mit der Kick als Seitenkette |
| `Body.h` (zwölf Moden, gestimmt) | Noctuary | "Boom" des Rumbles auf f0 (Dok. 8.4 Variante c), Resonanz der Pings | keine |
| `Cloud.h`, `GrainRing.h` | Noctuary | Granular-Soundscapes der Hypnotic-Mitte (Rødhåd, Dok. 3) | aus dem eigenen Chord-/Ping-Bus |
| `Modulation.h` (Lorenz, Rössler, Kuramoto) | Noctuary | Makro-Drift unter den LFOs | auf das Block-Parametersystem |
| `Tools/library/guide.py` (Fenster des Produktionsguides) | Noctuary | Vorbild: die Mix-Regeln von Dok. 8.7 als Fenster, durch die jedes Preset läuft | neu für Totality |
| `Plugin/`, `Quest/`, `Deploy/`, `Tests/` (TestSupport, vectest, vst3test, bench), `Tools/manual`, `UpdateCheck` | Ephemeris | Gerüste | Projektname, Pfade |

Nicht übernommen: aus Phosphene der Psytrance-Korpus, die Transformer-Gewichte, `Melody` (Constraint-Markov für Leads),
`TranceGate`, die SFX-Bank, `Vocal`; aus Ephemeris `Rack` (Sequenzer-Prozess), `TapeKeys`, `StringMachine`; aus
Noctuary `ClusterBrain`, `Cosmos`, `Memory`, `Tuning`, die Preset-Bibliothek. Die Field-Recordings aus Phosphene sind
eine Option für die Textur-Ebene (16.2).

## 5. Die Klangerzeuger

Jeder Erzeuger hat einen skalaren Referenzpfad und, wo es sich lohnt, einen Lane-Pfad. Velocity steuert bei jeder Stimme
auch die Klangfarbe (Cutoff, Decay, Drive), damit lautstärkekonstante Layer sich bewegen (Dok. 2, Brootle).

### 5.1 Kick (Zweischicht-Stimme, Dok. 3, 8.4)

- **Kern:** Phosphenes Kick mit zwei Engines (Sinus-Sweep in geschlossener Phase; Resonator als gedämpfter Zeiger, der
  wie die 808-Brücke Energie addiert statt neu zu starten) und einer dritten, **909-Engine**: Dreieck mit
  Tonhöhenhüllkurve, tanh-Shaper bis fast Sinus, paralleles tiefpassgefiltertes Rauschen und Klickimpuls (SOS Synth
  Secrets 34).
- **Rezept** [Q]/[A]: f_end = Kick-f0 45 bis 62 Hz; Tonhöhe startet 4- bis 16-fach höher (+24 bis +48 Halbtöne), τ_p
  10 bis 30 ms ("boomy" 180 bis 300 ms); Amplitude sofort, exponentiell −60 dB nach 300 bis 400 ms (Peak-Time) oder 400
  bis 600 ms (rolling); Klick 5 bis 15 ms Rauschen, Bandpass 2 bis 5 kHz; Waveshaper mit Drive 10 bis 40 %, mit ADAA
  erster Ordnung wie in Phosphene (27.09.2026 statt 4×, siehe Stand der Umsetzung); Tiefpass 12 dB bei 425 Hz; EQ HP 50
  Hz, −3 dB bei 500 Hz, Absenkungen 130 bis 250 Hz und 700 bis 1000 Hz. Umgesetzt: Tiefschnitt 30 Hz (ein HP bei 50 Hz
  läge fast auf dem Grundton) und die Senke bei 500 Hz; die übrigen Schnitte folgen mit der Kalibrierung.
- **Zwei Schichten** (Berghain-Rezept, Dok. 3): Top = 909-Engine, 8 Halbtöne tiefer, HP 400 Hz, −8 dB; Sub = Sinus oder
  Resonator, LP 120 Hz, etwa eine halbe Beatlänge.
- **Stimmung** (Dok. 4, 8.1): Tonika, wenn sie zwischen 41 und 62 Hz liegt (E1 bis B1); sonst die Quinte (C → G1 49 Hz,
  C# → G#1, D → A1, D# → A#1); b7 mit p 0,05. Phosphenes `tuneToKey` erledigt die Oktavwahl.
- **Ghost-Kick** 40 bis 60 Velocity; Sekundär-Kick auf den letzten zwei 16teln eines Beats, −4 Halbtöne, −10 dB (Dok.
  8.4, Big Beat) als Profiloption.
- **Kick-Notch:** −4,5 dB am Grundton der Bass-Linie, wenn Sub-Bass-Besitz (Dok. 8.4).

### 5.2 Rumble, frequenzgeteilt (Dok. 3, 8.4; Erg. 4)

```
 Kick-Körper (vor dem Klick) ─┬─ LR4-Weiche 80 Hz ─ oben ─► Hall (FDN oder Faltung, RT60 1–4 s, 100 % nass)
                              │                            ─► SoftClip (ADAA, 4×, 4–6 dB) ─► HP 80 Hz ─► LP f0·2…4
                              │                            ─► [Boom: Modalresonanz auf f0, optional]
                              └─ unten: Hüllkurve des Hallbands ─► Sub-Sinus auf f0, Phase = Fortsetzung der Kick
                                                                   an der Übergabe (outputPhaseAt)
 Summe ─► Ducker (Kick-Trigger, Attack 0–1 ms, Hold ≈ Kick-Länge, Release 150–350 ms, Tiefe 4–8 dB) ─► Band-Sättigung ─► Glue
```

- **Warum geteilt.** Jeder Hall dreht die Phase frequenzabhängig; über dem Kick-Grundton überlagert sich die Fahne
  zufällig mit dem Kick-Ausklang und kann ihn auslöschen (Erg. 4). Oberhalb 80 Hz ist das Klangfarbe, unterhalb Pegel.
  Unten spielt deshalb kein Hall, sondern ein Sinus, dessen Amplitude der Hüllkurve des Hallbands folgt und dessen
  Phase die der Kick fortsetzt: Phosphenes Kick kennt ihre eigene Ausgangsphase in geschlossener Form, der Sub setzt
  dort an. Das ist dieselbe Denkweise wie Phosphenes Bass-Split.
- **Hall-Rezept** [Q]: RT60 1 bis 2 s (TrackSensei) bis 3,79 s (Dark Cinematic), Predelay 0; Desktop Faltung mit einer
  Impulsantwort aus Noctuarys Bibliothek oder FDN, Quest FDN.
- **Varianten** (Dok. 8.4): (a) Ghost-Kick-Kanal mit eigenem Muster, Tiefpass und Einblendung; (b) Sekundär-Kick (5.1);
  (c) Boom auf f0 (`Body`, eine Mode); (d) unsynchronisiertes Delay L 2 / R 3, Feedback 50 %, vor dem Hall. Distortion
  vor dem Tiefpass = "rolling", danach = runder Sub.
- **Tiefenbesitz:** Rumble-Besitz = Kick plus Rumble sind die einzige Quelle unter 80 Hz, die Bass-Linie läuft
  hochpassgefiltert (ab etwa 100 Hz); Sub-Besitz = der Rumble verliert seinen Sub-Sinus und beginnt bei 80 Hz, der Bass
  hat den Sinus. Pegel Rumble oder Bass zur Kick −6 bis −10 dB [I].
- **Prüfung** (13.1): Energie von Kick plus Rumble in 30 bis 80 Hz über einen Beat nie unter der der Kick allein;
  Korrelation Kick-Ausklang gegen Rumble-Sub an der Übergabe über 0,9.

### 5.3 Sub und Bass (Dok. 4, 8.4, 8.6)

- **Sub:** Sinus, Bass-Riff −12 Halbtöne, nie unter 35 Hz, LP 80 Hz, mono; phasengekoppelt an die Kick (Phosphenes
  `KickLock`, in beide Richtungen wählbar).
- **Bass-Synth:** Säge oder Puls (SH-101/303-Idiom), HP 65 Hz, LP 350 Hz, Release etwa 300 ms, Phosphenes Bite für die
  Hörbarkeit auf kleinen Lautsprechern; Filter aus Ephemeris (Moog-Leiter, Diode für 303). 303-Linien als eigene Rolle
  im Ostgut- und Raw-Profil (Dettmann arbeitet mit 808, 909, SH-101, TB-303, Dok. 3).
- **Rhythmus:** 16tel-Ostinato aus der Matrix von Dok. 8.2 (keine Onsets auf 1/5/9/13), Formen Root-Ostinato mit
  Ersatzton auf Step 15/16, Oktavsprünge 1–5–8, 1–7–8, 1–5–9, 1–3–7.
- **Sidechain:** ereignisgesteuert, Attack 1 bis 10 ms, Release 150 bis 350 ms, 6 bis 12 dB, nur das Band unter 150 Hz.
- **Alternative** (Brootle): Sub aus dem Kick-Ausklang (Delay plus LP) oder aus dem Grain-Delay des Akkords −12 Halbtöne.

### 5.4 Das Kit: Hats, Ride, Clap, Snare, Toms, Rim, Shaker (Dok. 3, 8.4)

Phosphenes zwölf Lanes, mit neuen Rollen:

| Lane | Rolle | Quelle und Rezept [Q]/[A] |
|---|---|---|
| 1 | CH-Offbeat | 909-Metall: eine im Code erzeugte metallische Tabelle, auf 6 Bit quantisiert, mit Taktung 0,43 bis 1,7× abgespielt (die Tuning-Mod-Spanne der 909), exponentieller VCA, Decay 50 bis 80 ms, HP 7 kHz / BP 10 kHz; Velocity-Deckel 70 % |
| 2 | CH-16tel (Rolling-Hat) | wie 1, eigener Decay (kürzer bei leisen Schlägen); Choke-Gruppe mit 1 und 3 ("ein Hat je Step") |
| 3 | OH | wie 1, Decay 200 bis 600 ms, automatisiert (100 → 400 ms über 8 bis 16 Takte), vom nächsten CH gechoked |
| 4 | Ride | 808-Metall (sechs Rechtecke 205,3/304,4/369,6/522,7/540/800 Hz, BP 7100 und 3440 Hz), HP 1 kHz, Achtel laut/leise |
| 5 | Clap | Rauschen → BP 1,0 bis 1,2 kHz, hohe Güte → 3 bis 4 Neustarts im Abstand 10 bis 20 ms → Fahne 100 bis 300 ms; HP 300 Hz; Hall 10 bis 22 % |
| 6 | Clap-Ghost | wie 5, leiser (50 bis 70), gerollte Ghosts |
| 7 | Snare (tief) | 808-Snare −12 Halbtöne durch 24-dB-LP oder Tonoszillator ≈ 200 Hz plus Rauschen mit 50-ms-Filterhüllkurve |
| 8 | Rim | 909-Rim auf die Tonart gestimmt, Drive +6 dB, 24-dB-HP; −4 ms vor der Kick bei Koinzidenz (Dok. 8.3) |
| 9 | Shaker | Rauschen, HP, Resonanz plus Übersteuerung, Sustain automatisiert, 5 bis 15 ms spät |
| 10 | Tom | Sinus 8 Halbtöne über f0, Pitch-Mod 7 Halbtöne, Decay 350 ms → Hall → Bitcrusher |
| 11 | Conga | Modal (Raman-Membran), Tune to Key |
| 12 | Noise/FX | Rausch-Swell 8 bis 16 Takte vor einer Grenze, Crash/FX mit Trig-Condition 1:8 |

- **Verarbeitung** [Q]: CH durch LP mit 92 % Resonanz und +8 dB Drive (Dark Rumble), OH dunkel; Hats HP 300 bis 500 Hz;
  Delay-Modulation auf einem Hat (100 % nass, Sinus-LFO auf der Zeit); Bitreduktion auf Hats im Hypnotic-Profil.
- **Die Tiefenregel.** Phosphenes Kit schneidet unter 150 Hz, weil dort nur Kick und Bass spielen dürfen. Toms bei 8
  Halbtönen über einem f0 von 55 Hz liegen bei 87 Hz. Vorschlag [I]: Toms eine Oktave höher (≈ 175 Hz) als Standard;
  die tiefe Variante nur mit Kollisions-Dip und nie in Sub-Besitz-Tracks. Im Hörtest zu entscheiden.
- **Pegel zur Kick** [Q]: CH −8, OH −10, Conga/Perc −15, FX −10, Clap-Layer unter der Kick −30 dB.

### 5.5 Ping: FM und angeschlagenes Low-Pass-Gate (Erg. 1)

Die tonale Stimme der Hypnotic-Mitte, acht Stimmen in Lanes:
- **FM-Ping:** Sinusträger 200 bis 600 Hz mit schnellem Pitch-Decay (30 bis 90 ms); Modulator mit nicht ganzzahligem
  Verhältnis (√2, e ≈ 2,718, Primzahlverhältnisse wie 5:3, 7:4), Index 0,5 bis 3 aus der Velocity; Amp-Decay 40 bis
  180 ms. Phosphenes `PercKernel` hat Ton und FM bereits als Quelle; der Ping bekommt eigene Lanes, weil er gestimmt ist
  und ein eigenes Filter braucht.
- **LPG-Ping:** ein Impuls in ein Low-Pass-Gate, dessen Vactrol Filter und Verstärker gemeinsam öffnet und nichtlinear
  langsam schließt (Buchla 292; Modell nach Parker und D'Angelo, DAFx 2013). Mit hoher Resonanz eines Schaltungsfilters
  aus Ephemeris wird daraus der "Drip" oder "Clonk".
- **Danach:** Bandpass mit LFO-Sweep, Delay-Send, optional Granularwolke.
- **Rolle:** ungerades Ostinato (5 oder 7 Steps, oder Slipping 15/17) über dem 4/4, Töne aus dem Bass-Alphabet und der
  Skala, höchstens zwei Tonhöhenklassen je Motiv; Motivwechsel nur an 4/8/16-Takt-Grenzen.

### 5.6 Dub-Chord und Stab (Dok. 3, 8.4)

- **Stimmen:** zwei Sägezähne ±5 bis 15 Cent je Akkordton, drei Töne (0/+3/+7 oder m7), optional eine Oktave tiefer;
  Amp A ≈ 10 ms, D 200 bis 400 ms, S 15 %; Filterhüllkurve 72, Velocity auf die Hüllkurve. Ephemeris' `ModVoice` mit
  VCO-Modell Prophet (historisch: Prophet-5-Akkorde bei Basic Channel, Dok. 3) oder Juno.
- **Filter:** Bandpass oder Hochpass 280 bis 450 Hz, Resonanz niedrig, Sinus-LFO 0,1 bis 0,3 Hz ±0,5 Oktaven.
- **Kette** (Basic-Channel-Farbe, Dok. 3): Bitreduktion 6 bis 8 Bit zu etwa 20 %, Vinylknistern; Phaser vierstufig 0,3
  bis 1 Hz; Delay 1 punktierte Achtel (346 ms bei 130), Feedback 20 bis 30 %, HP 200 Hz und LP 4 bis 5 kHz in der
  Schleife; Delay 2 frei 264 ms mit 85 % Feedback oder 2/16-Ping-Pong und 3/16; Platte RT60 3,8 bis 5 s, 30 bis 40 %;
  EQ −7 dB bei 600 Hz, HP 100 Hz, LP 4,5 kHz; drei bis vier Sättigungsstufen zu 10 bis 20 %; Sidechain zur Kick.
  Ephemeris' Bandecho mit Feder ist die RE-201-Hälfte der Kette ("dirty preamps, spring reverb, noisy tape delay").
- **Voicing:** enge Lage, Block-Transposition (Chord-Unit 0/+3/+7), Terzen über 130 Hz, besser über 200 Hz (Dok. 4).
- **Rolle:** ein wiederholter Akkord (Pheek: "reverb is 50 % of your job"), Positionen aus der Stab-Matrix (Dok. 8.2),
  lauter werdend über einige Takte ("nudging louder into the mix every few bars", Dok. 5).

### 5.7 Drones und Textur

- **Drone:** gehaltene Modularstimme auf Tonika oder Quinte über 150 Hz, Filterfahrt über 32 bis 64 Takte; im
  Hypnotic-Profil häufig, sonst selten.
- **Textur** (Dok. 8.4) [Q]/[I]: vierstimmiger Loop aus Vinylknistern (Impulsprozess mit Bandpass), Netzbrummen (50 Hz
  mit Obertönen, gegen die Tonart gestimmt oder um einige Cent daneben), Rausch-Erosion; −20 bis −30 dB. Noctuarys
  `Patina` für die Alterung des Busses.
- **Granular** (Rødhåd, Dok. 3): Noctuarys `Cloud` aus dem Ping- und Chord-Bus für Soundscapes im Intro und in
  Reduktionen.

### 5.8 Modulation, global und frei laufend (Dok. 8.4)

Mindestens drei LFOs mit inkommensurablen Perioden (7, 11, 13 Beats ≈ 3,2/5,1/6,0 s bei 130) auf Cutoff (±10 bis
20 %), Drive oder FM-Index, Delay-Send, Hat-Decay; S&H je Step auf Velocity (±15) und Pan; Filter-LFOs 0,05 bis
0,08 Hz (12 bis 20 s); Hat-Lautstärke-Sinus 10 bis 20 %; Delay-Zeit-LFO mit einer Takt-Periode. Phasen aus der
absoluten Beat-Position, nie aus einem Zähler (blockgrößenunabhängig, Takt allein gleich Takt in Folge). Unter den LFOs
eine Makro-Drift aus Noctuarys gekoppelten Oszillatoren (Kuramoto), damit sich über 30 Minuten nichts periodisch
wiederholt.

### 5.9 Raum und Dub-Effekte

| Effekt | Verfahren | Herkunft |
|---|---|---|
| Dub-Delay | Tempo-Delay 1/8, 3/16, 1/4 mit HP 200 Hz und LP 5 kHz in der Schleife, Wow und Flutter, Feedback 30 bis 90 % als Automation ("edge of self-oscillation"); Delay-Wurf als Ereignis | Phosphene `TempoDelay`, Ephemeris `TapeEcho` |
| Feder | Allpass-Dispersionskaskaden | Ephemeris `Spring` |
| Platte | Dattorro | Ephemeris `Plate` |
| Hall | FDN acht Linien; Faltung mit erzeugten IRs, Frühreflexionen als SDN | Ephemeris `Reverb`, Noctuary `Convolution`, `EarlyRoom` |
| Phaser | ZDF, vierstufig | Phosphene `PsyFx` |
| Lo-Fi | Bitreduktion, Sample-Rate-Reduktion mit Antialias-Filter | neu (klein) |

Drei Sends je Deck (Dub-Delay, Feder/Platte, Hall), Hallrückwege HP 200 bis 400 Hz (Dok. 8.7), Rückwege gegen die Kick
spektral geduckt (Noctuary `Unmask`).

## 6. Die Pattern-Ebene

### 6.1 Das Rack

- **Layer:** Kick, Ghost-Kick, Rumble, CH-Offbeat, CH-16tel, OH, Ride, Clap, Clap-Ghost, Snare, Shaker, Tom/Conga, Rim,
  Bass, Ping, Stab, Noise. Jeder Layer: Loop-Länge 1, 2 oder 4 Takte (p 0,10/0,55/0,35, Dok. 8.2), eine Matrix von
  16 × Takte Steps mit P(Onset), Velocity-Bereich, Offset, Swing-Anteil, Trig-Condition.
- **Anker und Motion** (Myloops, Dok. 5): Kick, Sub und CH-Offbeat sind innerhalb eines Blocks Anker (identisch jeden
  Takt, Wahrscheinlichkeit 1); alle Werte unter 1 werden jeden Takt neu gewürfelt (Ghost 40 bis 70 %, Spine 100 %,
  Velocity ±15 bis 20). Mutation der Muster selbst nur in Takt 2 bzw. 2 und 4 eines Loops.
- **Der Kern-Zustand** ist die Matrix von Dok. 8.2 (voller Groove). Intro und Outro skalieren die Nicht-Anker auf 0
  oder halbe Dichte; der Komponist wählt Layer und Dichte, nie einzelne Noten.

### 6.2 Regeln (Dok. 8.2, hart)

Lücken-Reservierung (1/5/9/13 nur Kick, Clap, Rim; keine Bass-Onsets dort); ein Hat je Step (OH von der nächsten CH
gechoked); Kollisions-Dip (Perc gleichzeitig mit Kick oder Hat: Velocity −25 % oder −4 ms); Dichte-Deckel 6 bis 8
simultane Layer (Minimal 4, Hypnotic-Profil 5); Fill als 16tel-Roll am Phrasenstart alle 8 Takte mit p 0,25, keine
Snare-Rolls.

### 6.3 Ghost-Ketten (Erg. 7)

Die Wahrscheinlichkeiten von Dok. 8.2 bleiben die Grundlage; für Ghost-Kick, Clap-Ghost, Tom/Conga und Shaker werden sie
mit bedingten Faktoren multipliziert:

  P(Step n) = p_n · f(Step n−1, Step n−2 desselben Layers) · g(gleichzeitige Onsets anderer Layer)

- f senkt nach einem Ghost die Chance der nächsten zwei Steps (keine Cluster), g senkt sie, wo schon ein anderer Ghost
  oder Perc-Onset liegt, und hebt den Push vor Beat 4, wenn Beat 2 ohne Ghost blieb (Erg. 7).
- Die Kette läuft über Taktgrenzen weiter; ihr Zustand ist Teil dessen, was der Komponist vorausschreibt, damit ein Takt
  allein dieselben Noten ergibt wie in der Folge.
- Werte [I], Startpunkt: f = 0,5 für den Nachbar-Step, 0,75 für den übernächsten; g = 0,5 bei Koinzidenz, 1,5 für den
  Push. Kalibrierung über den Hypnose-Korridor und die Onset-Profile der Referenzen (13.4), da es keine Stems gibt.

### 6.4 Polymeter, Euklid, Displacement, Slipping

- **Polymeter** (Dok. 8.2): ein bis zwei Perc-Layer mit Periode 3, 5, 6, 7 oder 12 Steps (p 0,5 je Track), Reset an
  16-Takt-Grenzen; Realignierung nach kgV(p, 16)/16 Takten (3, 5, 3, 7, 3).
- **Euklid:** E(3,8), E(5,16), E(7,16) mit einer Rotation, die keinen Onset auf 1/5/9/13 legt; Rotation nach der
  LHL-Syncopation in die Mitte zwischen der geringsten und der stärksten (Phosphene `Rhythm`, Sioros et al.).
- **Displacement** (Butler): statt 3-gegen-4 den ganzen Layer um 1 bis 3 16tel verschieben, p 0,3 je Perc-Layer.
- **Slipping** (Erg. 5): Loops mit N = 15 oder 17 Steps gegen den Mastertakt; ein 15er-Loop beginnt jeden Takt ein
  16tel früher und steht nach **15 Takten** (240 Steps) wieder auf der Eins, ein 17er nach **17 Takten**; 31 und 33 als
  langsame Variante (31 bzw. 33 Takte). Bei 130 BPM sind 15 Takte 27,7 s. Slipping-Layer laufen innerhalb eines Blocks
  frei und setzen an Blockgrenzen neu auf; der Komponist kennt ihre Konjunktionen und legt Mikroereignisse bevorzugt
  dorthin. Anteil je Profil: Hypnotic hoch, Dub und Raw niedrig.

### 6.5 Trig-Conditions (Dok. 7, 8.2)

Elektron-Formalismus: A:B (spielt im A-ten von B Durchläufen: OH-Extra 2:4, Crash/FX 1:8, Shaker-Einzelnote 3:4), FILL,
PRE/NEI (abhängig vom vorigen Trig desselben oder des Nachbar-Layers), 1ST, LST.

### 6.6 Velocity und Timing (Dok. 8.3)

Die Tabelle von Dok. 8.3 gilt wie sie steht (Kick 127 fest, Ghost-Kick 40 bis 60, CH-Offbeat 80 bis 90 mit Deckel 89,
CH-16tel Basis 64 mit Akzenten, OH 64 bis 90, Ride 100/70, Clap 110 bis 127, Clap-Ghost 50 bis 70, Shaker/Tom/Conga 70
bis 100, Rim 70 bis 90, Bass 90 bis 110, Stab 79 % mit Zufall 43 %). Swing nach MPC-Skala 50 bis 58 %, Standard 53 %,
nur auf Layer mit Swing-Anteil, der Bass mit halbem Betrag; Zufallsversatz bis 10 ms Standardabweichung, nie auf die
Kick, nie früh bei Snare und Clap (Frühauf: früh ist schlechter als spät). Dub-Profil: Swing 50 %, Bewegung aus dem
Delay. Alternativ das Akzentmodell A(i) = C^L(p_i) mit C ≈ 0,7 bis 0,85.

### 6.7 Mininotation

Jedes Pattern lässt sich als Tidal/Strudel-Mininotation ausgeben (`bd*4`, `[~ oh]*4`, `perc(5,16,2)`, `hh*16?0.3`,
Dok. 8.0): `tot_render --patterns` schreibt sie je Layer und Block. Damit sind Muster lesbar, vergleichbar und in
Strudel hörbar, ohne den Generator zu starten.

## 7. Der Komponist

### 7.1 Ebenen

1. **Set** (1 bis 8 Stunden): Zahl der Tracks, Dramaturgie (Warm-up, Peak, Closing, Sonntag, Flach), Tempo- und
   Energiebogen, Profilreise, Übergänge (7.7).
2. **Track** (6 bis 9 Minuten, 192 bis 256 Takte): Formtyp, Tonart und Kick-f0, Tiefenbesitzer, Layer-Vorrat mit
   Einsatzreihenfolge, Klänge (Programmwechsel), Hypnose-Korridor.
3. **Block** (32 Takte): genau eine Operation, Automationsrampen, Ereignisse auf 8/16-Takt-Rastern.
4. **Takt** (Motion-Würfe, Ghost-Ketten) und **Step**.

### 7.2 Formtypen und Grammatik (Dok. 8.5, Erg. 8)

```
Track      := Arc | Peak | Endless
Arc        := Intro Body Outro                  # "tool": kein oder ein kurzer KickOut
Peak       := Intro Body Reduction Body' Outro  # ein oder zwei Höhepunkte, nie mehr
Endless    := Core{192..256}                    # voller Druck ab Takt 1, abruptes Ende im Loop
Intro      := BeatBlock{32 | 64}                # Kick (+Rumble), CH, eine Perc; kein Bass, kein Stab
Body       := Block{32}^(4..6)
Block      := Op + Events + Automation
Op         := Add | Remove | Swap | Hold        # genau eine je Block ("staircase")
Outro      := BeatBlock{32 | 64}                # Spiegel des Intros, subtraktiv
Reduction  := KickOut{4 | 8 | 16} [Swell{8 | 16}] Return
Events     := alle 8 | 16 Takte: MuteOneHit | OneBarDropout | DelayThrow | Fill(selten)
Automation := Ramp(16 | 32 Takte, blockbündig) | Drift(ganzer Track) | LFO(0,05–0,08 Hz, frei)
```

- **Regeln** (Dok. 8.5) [Q]/[A]: alle Ereignisse auf Vielfachen von 4 Takten, große Wechsel (Bass, Reduction, Return)
  auf 16/32; Intro 32 (p 0,6) oder 64 (0,4); Einsatzreihenfolge Kick → Hats/Ride → Clap/Perc → Bass → Stab/Chord →
  Pad/Textur; mindestens zwei Elemente erst in der zweiten Hälfte; Reduction im Tool ∅ (0,5) oder KickOut 4 bis 8
  (0,5), im Peak KickOut 8 (0,5) / 16 (0,35) / 32 (0,15) bei 50 bis 65 % der Body-Länge, danach der dichteste Block
  32 bis 64 Takte; Swell 8 bis 16 Takte vor der Grenze; Cut als Ein-Takt-Mute oder "mute one expected hit" direkt vor
  der Rückkehr; keine Tonalität in den ersten und letzten 32 Takten (außer Endless).
- **Endless** (Erg. 8): kein Intro, kein Outro, kein Mute, keine Reduction; Kick, Bass und Tops ab Takt 1; die ganze
  Dynamik liegt in Meso- und Mikro-Automation (Filterfahrten über 64 Takte, asynchrone Phaser, LFOs) und in den
  Slipping-Layern. Im Set nur als zweites Deck über einem laufenden Track oder nach einem Track mit langem Outro (7.7).
- **Dichteprofil** [A]: Tool 0,3 (Takt 1 bis 32) → 0,6 → 0,85 → 1,0 → 0,6 → 0,3; Peak gleich bis Takt 128, dann ein
  Einschnitt auf 0,3 bis 0,5 für 8 bis 32 Takte, 1,0 für 32 bis 64 Takte, Outro.
- **Fitness** (Eigenfeldt): Strafen für leere Phrasen, mehr als eine Op je Block, Tonales in Intro oder Outro.

### 7.3 Automation (Dok. 8.5, Tabelle)

Perc-Bus-LP 800 Hz → offen über 32 Takte; Master-/Gruppen-HP 20 → 200 bis 400 Hz und zurück über 8 bis 16 Takte;
Stab-Filter ±1 Oktave um 350 Hz über 16 bis 32 Takte; Delay-Feedback-Wurf 30 → 85 → 30 % über 1 bis 2 Takte am
Phrasenende; Reverb-Send auf Rumble und Stab −∞ → −12 dB über einen Block; OH-Decay 100 → 400 ms über 8 bis 16 Takte;
freie LFOs ±10 bis 20 % bei 0,05 bis 0,08 Hz; Track-Drift Cutoff/Resonanz +20 % über den ganzen Track; Rausch-Swell
−∞ → −12 dB 8 bis 16 Takte vor einer Grenze. Die Kurven kommen aus Ephemeris' Gesten-Engine (Minimum Jerk, höchstens
zwei Hände zugleich), damit die Fahrten nach Händen klingen und nicht nach Linealen.

### 7.4 Mikroereignisse (die Hypnose-Ebene)

Unterhalb der Block-Ops ein stetiger, kleiner Strom: ein OH, das um wenige Prozent länger wird; ein fehlender Schlag;
ein Delay-Wurf auf einem einzigen Ping; ein Hat, der für einen Takt ins Delay kippt; die Rotation eines Polymeter-Layers
um ein 16tel; ein Ghost mehr. Rate und Größe kommen aus dem Hypnose-Korridor des Profils (7.9), die Orte bevorzugt aus
den Konjunktionen der Slipping-Layer und den 4-Takt-Grenzen.

### 7.5 Harmonie (Dok. 8.6)

Wie in 2.6; dazu das Dissonanz-Budget (b2-Nachbar, Tritonus-Alternation, {0123}-Cluster, ±5 bis 15 Cent Detune,
Delay-Drift) und die Harmonik-Prüfung von Dok. 8.9 (pc-Set-Kardinalität ≤ 4, Bass ≤ 2, kein Leitton, alles in der
Tonart) als harte Regel. Jeder Track gibt sein Camelot-Label aus ("A" oder "monotonic").

### 7.6 Stilprofile

Die vier Profile von 2.8 als Werte mit Namen (designierte Initialisierer wie Ephemeris' `Style.h`), Interpolation
zwischen zwei Profilen, dazu die Achsen Dub-Anteil und Hypnotic-Anteil. Jedes Profil trägt seinen Hypnose-Korridor und
sein Lautheitsziel.

### 7.7 Das Set: DJ-Übergänge und Live-Rekomposition (Dok. 6, 8.8)

- **Tempo:** monoton, höchstens 1 BPM je Track, Bogen je Dramaturgie (etwa 126 → 134 über einen Abend, Dok. 6:
  Berghain 06 steigert bis 134); die Rampe liegt im Blend.
- **Blend:** 16 bis 32 Takte (Korpus-Gipfel bei 8, für Berlin eine Untergrenze), Einstieg auf dem ersten Downbeat eines
  16/32-Takt-Low-Segments, **harter Bass-Swap exakt an einer 32-Takt-Grenze** über den Isolator (Kill des tiefen Bands
  am abgehenden Deck, Öffnen am kommenden im selben Sample), Cue-Marke dort.
- **Keine Transposition**; Tonart-Verträglichkeit ist sekundär gegenüber dem Rhythmus (Dok. 6), Camelot ±1 bevorzugt.
- **Sequenzierung** nach Energie und Klangfarbe (Bittner et al.: kürzester Pfad über Timbre, Key, Tempo).
- **Live-Rekomposition** (Rødhåd: "een track loopt, op de andere een loop"; DVS1: "I take multiple pieces of beats and
  rhythms and I layer them"): ein drittes Deck spielt einen 4- oder 8-Takt-Loop (Hats, Perc, Ping) eines früheren Tracks
  unter dem aktuellen weiter, oder ein Endless-Track läuft als Werkzeug unter einem Arc-Track; die Tiefenregel gilt über
  alle Decks (nur ein Deck besitzt das Band unter 150 Hz). Breaks per Effekt (Hall, Delay am DJ-Send) auch dort, wo der
  Track keinen hat.
- Im VST3 gilt das Host-Tempo; die Tempo-Karte des Sets kommt über den MIDI-Export in die DAW (wie Ephemeris).

### 7.8 Sperren und Neuwürfeln

Set, Track, Block, Layer (Pattern), Klang und Automationsspur sind einzeln sperrbar und haben je einen Seed-Zweig. Neu
würfeln ersetzt nur Ungesperrtes. Ein gesperrter Layer behält seine Matrix, auch wenn der Block um ihn herum neu
gewürfelt wird.

### 7.9 Kandidaten und der Hypnose-Korridor

Für jeden Block erzeugt der Komponist mehrere Kandidaten (Standard 8) aus Op, Ereignissen und Motion-Seeds und berechnet
auf der Partitur: Entropierate und PIR je Band (2.9), Mikroveränderungsrate, LHL-Syncopation je Stimme, polyphone
Syncopation (Witek), Euklidische Gleichverteilung, Dichte. Gewählt wird der Kandidat, der dem Zielpunkt des Profils im
Korridor am nächsten liegt; harte Regeln (Dichte-Deckel, Form, Harmonik) sind Filter, keine Kosten. Das ist die
Fitness-Idee von Eigenfeldts GERP (Dok. 7), angewandt je Block statt über den ganzen Track, damit der Komponist im
Vorauslauf bleibt. Die symbolischen Größen entsprechen den Audio-Größen der Referenzmessung (13.4), damit Soll und Ist
dieselbe Einheit haben.

## 8. Mix und Master (Dok. 8.7)

### 8.1 Kanalzüge und Pegel

Referenzpegel [Q]: Kick 0 dB, CH −8, OH −10, Conga/Perc −15, FX −10, Clap-Layer unter der Kick −30, Sub-Layer −8
(Start). Hochpässe: Synths 150 bis 200 Hz, Hats 300 bis 500 Hz, Claps 300 Hz, Hallrückwege 200 bis 400 Hz, Master 20
bis 30 Hz mit 24 dB/Oktave (Basic-Channel-Rezept unter 37 Hz). Die Pegel sind Startwerte des Klangs, der Leveler (8.5)
setzt den Track.

### 8.2 Stereo und Tiefe

Unter 120 bis 150 Hz strikt mono (Noctuary `MidSide`), Sub-Band-Korrelation 1,0; Stereobreite über 200 Hz 0,16 bis
0,24 (15 bis 40 %); Body 120 bis 500 Hz kontrolliert, darüber breit; Phosphenes Auto-Pan (Drei-16tel-Periode) für die
Kit-Lanes.

### 8.3 Ducking

Ereignisgesteuert vom Kick-Trigger (Phosphene `Ducker`): Attack 1 bis 5 ms, Release 150 bis 350 ms (höchstens 462 ms
bei 130), Tiefe 4 bis 8 dB Rumble, 6 bis 12 dB Bass, nur die ersten 100 bis 150 ms. Multiband-Variante für Pads und
Hallfahnen: 20 bis 200 Hz 8 bis 12 dB, 200 Hz bis 2 kHz 2 bis 4 dB, darüber 0.

### 8.4 Bus

Drei bis vier Sättigungsstufen zu 10 bis 20 % (Drum-Buss-Idiom, "Drive 17 %, Out −3 dB"), jede 4× überabgetastet; Glue
1 bis 2 dB Gain Reduction, Attack 10 bis 30 ms (Master-Glue: Schwelle −16 dB, 35 % Mix).

### 8.5 Lautheit und Leveler

Ziel −11 bis −10 LUFS integriert, Short-Term etwa −8 in den Hauptteilen, True Peak ≤ −0,5 dBTP (der Korpus liegt über
0 dBTP, das wird nicht nachgeahmt); Crest 8 bis 12 dB (Minimal/Dub), 6 bis 8 dB (Peak). Vorschlag je Profil [I]:
Hypnotic −11, Ostgut −10,5, Dub −11,5, Raw/Peak −10. Anders als bei den Geschwistern ist der Master hier ein
Klangmittel: Glue, ein weicher Clipper (ADAA, 4×) und ein True-Peak-Limiter mit Lookahead; Becker folgend kein
Limiting in den Kanalzügen und Bussen. Ephemeris' Leveler misst den lautesten Teil jedes Tracks und setzt die Korrektur
(höchstens ±4 dB), live und offline gleich.

### 8.6 Spektrale Ziele, Tilt und das Cut-Profil

- **Ziele** [Q] (TrackSensei, Trackscore, Vendor): Sub 20 bis 60 Hz ≈ 40 % und Bass 60 bis 250 Hz ≈ 45 bis 52 % der
  Tiefton-Energie; über 5 kHz ≈ 7 %; Schwerpunkt 1,5 bis 3,5 kHz. Gemessen an den Referenzen (13.4) vor dem Einbau.
- **Tilt** (Erg. 2): ein Master-Regler, dessen Standard aus dem Abstand zwischen Totality und Referenzen über 3 kHz folgt,
  nicht aus einer festen Kurve.
- **Cut** (Erg. 3, Dok. 8.7): optionales Master-Profil für Vinyl: Seite höchstens 12 bis 15 Minuten bei 45 U/min, Mono
  unter einer Weiche (150 Hz), keine Gegenphase 80 bis 300 Hz, dynamische Höhenbegrenzung 6 bis 10 kHz (die
  Schutzschaltung des Schneidkopfs), LP 16 kHz, Noctuarys `Patina` für die Lackfarbe.

## 9. Ausgaben: MIDI, Stems, Cues, Dateien (Dok. 8.8)

- **SMF Format 1**, PPQ 960, Tempo-Karte mit der Drift des Sets, Marker für Blöcke, Ops, Reduktionen, Tracks.
- Ein Track je Layer (Kick, Ghost-Kick, Hats, Clap … mit GM-Drumnummern, damit eine DAW sie auf einem Drum-Rack
  erkennt), dazu Bass, Ping, Chord, Drone; Automationen als CC-Verläufe (74 Cutoff, 71 Resonanz, weitere frei) in
  1/32-Auflösung.
- **Stems** je Erzeuger und Send aus dem Offline-Render; **DJ-Stems**: Kick-only, Hats-only, Perc-only und loopbare
  4/8-Takt-Segmente (Rødhåd, DVS1: Element-Borrowing).
- **Cue-Marken:** Intro-Ende, Bass-Einsatz (Bass-Swap-Punkt auf der 32-Takt-Grenze), Reduktionen, Outro-Beginn; als
  RIFF-`cue `/`LIST adtl`-Chunk in der WAV und als JSON daneben. Ob Rekordbox und Traktor den WAV-Chunk lesen, ist zu
  prüfen (15.8).
- **`.totset`** (Text wie `.ephset`): Seed, Profil(e), Dramaturgie, Sperren, auf Wunsch die ausgerollte Partitur.
- **Presets:** Klänge je Stimme (Programmwechsel), Pattern-Sätze, Stilprofile, Set-Dramaturgien.

## 10. GUI

### 10.1 Desktop (JUCE, Layout aus Parametern, `TOT_SHOT`-Screenshot-Modus, Englisch)

| Tab | Inhalt |
|---|---|
| **Set** | Länge, Dramaturgie, Profil(e) mit Morph und den Achsen Dub/Hypnotic, Tempobogen, Seed, Generate/Play/Stop, Meter |
| **Arrange** | Zeitleiste Track → Blöcke mit ihren Ops, Part-Aktivierungsmatrix (Layer × 8-Takt-Phrase), Reduktionen, Sperren, Neuwürfeln, Sprung |
| **Patterns** | jeder Layer als 16-Step-Raster mit Wahrscheinlichkeit, Velocity, Offset, Condition; dazu die **Eclipse-Ansicht**: die Kick als dunkle Scheibe in der Mitte, jeder Layer ein Ring um sie, Polymeter- und Slipping-Ringe präzedieren sichtbar und leuchten an ihren Konjunktionen auf |
| **Low End** | Kick (drei Engines, zwei Schichten), Rumble mit Weiche und Hall, Sub, Tiefenbesitz; eine Phasenansicht der Übergabe Kick → Rumble-Sub |
| **Drums** | die zwölf Lanes, Swing, Timing, Velocity |
| **Tones** | Ping, Bass, Chord, Drone, Textur |
| **Dub** | die Kette: Lo-Fi, Phaser, Delays, Bandecho, Feder, Platte; Wurf-Tasten |
| **Mixer / Master** | Kanalzüge, Sends, Ducking, Bus, Master, Lautheit (LUFS, Short-Term, True Peak, Crest), Tilt, Cut |
| **Perform** | live wie am Mischpult: Layer muten und entmuten, Isolator-Kills je Deck, Filter greifen, Delay-Wurf, Loop auf das dritte Deck legen; MIDI-Learn |
| **Export** | MIDI, Stems, DJ-Loops, Cues, `.totset` |
| **Style** | Profile ansehen, editieren, Korridore und Messwerte der Referenzen |

### 10.2 Quest 2

Der komplette Generator läuft auf dem Gerät (Entscheidung 16.1). Die Hände sind die Hände am Mischpult: linke Hand
Filter und Send des Layers im Fokus, rechte Hand Mute/Unmute und Delay-Wurf, Pinch auf einen Ring der Eclipse-Ansicht
nimmt diesen Layer. Im Raum wird die Eclipse zur verdunkelten Scheibe über dem Spieler, die Ringe kreisen um sie.
Handmenü wie in Noctuary Quest.

### 10.3 Kaleidoscope-Kopplung

OSC `/tot/beat`, `/tot/bar`, `/tot/block`, `/tot/op`, `/tot/event`, `/tot/conjunction`, `/tot/key` aus `Cue.h`.

## 11. Vektorisierung und CPU

- Prinzip wie die Geschwister: Structure-of-Arrays über Stimmen und Lanes, skalarer Referenzpfad als Orakel, bitgleiche
  Tests.
- **Natürliche Lane-Gruppen:** zwölf Kit-Lanes (zwei AVX2- oder drei NEON-Register, aus Phosphene), acht Ping-Stimmen,
  sechs Akkordstimmen mit zwei Oszillatoren, das FDN.
- **Grober CPU-Rahmen** (Desktop, ein Kern, 48 kHz, ein Deck; in Phase 1 bis 3 zu prüfen):

| Modul | Ziel |
|---|---|
| Kick (4× Shaper) und Rumble (Faltung oder FDN, 4× Clip) | < 2 % |
| Kit, zwölf Lanes | < 1,5 % |
| Ping, acht Stimmen | < 1 % |
| Bass (Schaltungsfilter 2×) | < 1 % |
| Chord (sechs Stimmen, Schaltungsfilter 2×) | < 3 % |
| Dub-Kette, Sends, Hall | < 3 % |
| Drone, Textur, Granular | < 1,5 % |
| Bus und Master (4× Sättigung, Clipper, TP-Limiter) | < 2 % |
| **Summe je Deck** | **< 15 %** |

- **Blends verdoppeln** die Last für 16 bis 32 Takte (zwei Decks), das dritte Deck (Loop) kostet nur, was es spielt.
- **Quest-Stufe:** Rumble mit FDN statt Faltung, Oversampling nur an Kick-Shaper und Master-Clipper, Granular aus,
  Akkord mit einem Oszillator je Ton; im Blend spielt das abgehende Deck in der niedrigen Stufe. Auf dem Gerät messen.

## 12. Plattformen und Build

- Aufbau wie Ephemeris: `Core/`, `Plugin/` (JUCE, VST3 + Standalone), `Quest/`, `Tools/render` (`tot_render`:
  Offline-Render, `--bench`, `--midi`, `--stems`, `--loops`, `--cues`, `--patterns`, `--set-file`, `--seed`, `--style`,
  `--minutes`, `--tracks`), `Tools/*.py`, `Tests/`, `Deploy/`, `docs/`.
- CMake-Optionen `TOT_BUILD_PLUGIN`, `TOT_BUILD_TOOLS`, `TOT_AVX2`, `TOT_STATIC_RUNTIME`; kein Fast-Math.
- **VST3:** Host-Playhead als Takt, Host-Tempo gilt (7.7). **Standalone:** eigene Uhr, ASIO/WASAPI, Recorder, Exporte,
  Sets mit Drift. **Quest:** `TOT_MUTE=1` für Tests.
- Update-Prüfung einmal am Tag wie Ephemeris; Installer (Inno Setup) und Release-Skripte aus Ephemeris.

## 13. Tests und Messungen

### 13.1 Selbsttest (`tot_selftest`, Muster Ephemeris)

- **Kick:** geschlossene Phase gegen numerisches Integral; 909-Engine alias-arm (4×); Stimmung in 41 bis 62 Hz, Quinte
  für C bis D#; Tail-Limit.
- **Rumble:** Energie Kick plus Rumble in 30 bis 80 Hz nie unter der Kick allein (je Beat, über Kick-f0 45 bis 62 Hz
  und RT60 1 bis 4 s); Korrelation an der Übergabe über 0,9; Weiche summiert sich flach.
- **Ducking:** Kurve sample-genau am Trigger, blockgrößenunabhängig.
- **Pattern-Rack:** Lücken-Reservierung, ein Hat je Step, Kollisions-Dip, Dichte-Deckel nie verletzt; Anker jeden Takt
  gleich; Polymeter-Realignierung nach kgV; Slipping 15 und 17 auf der Eins nach 15 bzw. 17 Takten; Ghost-Ketten
  deterministisch und ein Takt allein gleich dem Takt in der Folge; Trig-Conditions nach Definition.
- **Form:** eine Op je Block; Grenzen mod 4, große Wechsel mod 16/32; Intro und Outro ohne Tonales (außer Endless);
  höchstens zwei Peaks.
- **Harmonie:** pc-Set ≤ 4, Bass ≤ 2, kein V-i, kein Leitton, unter 150 Hz nur Unisono/Oktave/Quinte.
- **Set:** Bass-Swap exakt auf der 32-Takt-Grenze; Tempo monoton, höchstens 1 BPM je Track; nie zwei Tiefenbesitzer.
- **Allgemein:** Blockgrößen 1/37/512 bitgleich; MIDI-Rundlauf; Lautheit und True Peak im Ziel; Mininotation
  rundläuft (schreiben, lesen, gleiche Matrix).

### 13.2 Vektor-, Host- und Build-Tests

Aus Ephemeris: `vectest` in drei Builds (AVX2, NEON-Shim, skalar), `hosttest`, `vst3test`, `bench`, `questguard`,
pluginval auf Strenge 10.

### 13.3 Hörprüfung

Solo-Renders je Erzeuger; A/B zweier Seeds; ein 60-s-A/B gegen die Referenzen bei gleicher Lautheit (Dok. 8.9: kein
EDM-Generator berichtet einen formalen Hörtest; schon dieser Vergleich wäre ein Beitrag); ein Blend-Test über 32 Takte
gegen einen Referenztrack.

### 13.4 Referenzmessung

**Material.** Die Tracks aus Dok. 1 (Beatport-Tempo und -Tonart dort angegeben) über YouTube (Entscheidung 16.1),
ergänzt um Hypnotic- und Dub-Referenzen, weil das Dokument für die Mitte kaum Titel nennt. Vorschlag, vor dem Laden
mit dem Nutzer abzustimmen:

| Profil | Titel (Tempo und Tonart nach Dok. 1, wo vorhanden) |
|---|---|
| Ostgut | Klock & Dettmann *Dawning* (125, Eb-Moll); Dettmann *Quicksand* (125); Len Faki *Rainbow Delta* (129); Function *Disaffected* (127); Ben Klock *Subzero* (125); Sandwell District *Immolare (Function Version)* (127); Dettmann *Deluge* (130); Norman Nodge *Maniac* (127); Efdemin *Decay* (130); Rødhåd *Haumea (Dirt Mix)* (130); Head High *Rave (Dirt Mix)* (128) |
| Dub | Quadrant *Infinition* (128); dazu Vorschlag: Basic Channel *Quadrant Dub*, Porter Ricks (*Biokinetics*), Monolake (*Cyan*) |
| Raw/Peak | PAS *Bell Blocker* (129); Jeff Mills *The Dancer* (131); PAS *Kat* (132); DVS1 *Running* (135); Kobosil *Per* (135); Oscar Mulero *Generator* (135) |
| Hypnotic | Vorschlag: Donato Dozzy (*K*), Voices From The Lake (*Voices From The Lake*), Rrose (*Waterfall*), dazu Titel von Mike Parker, Oscar Mulero, Luigi Tozzi, Shifted aus Prologue, Semantica, Pole Group, Mote-Evolver; Auswahl gemeinsam |

Gegenpol *Legacy* (160 BPM) bleibt draußen.

**Verfahren.** `Tools/fetch_refs.py` liest `Tools/ref_sets.txt` (Profil, Künstler, Titel, URL), lädt mit `yt-dlp` nur
die Tonspur in einen Ordner außerhalb des Repos (`%TEMP%\totality_refs`), dekodiert mit `ffmpeg`, und die Messwerkzeuge
schreiben nur Statistiken (`Tools/ref_stats.json`); das Audio wird danach gelöscht, sofern der Nutzer nicht anders
entscheidet. Vor jedem Laden legt der Plan die konkrete Liste mit Quelle vor (16.2).

**Vorbehalte.** YouTube liefert verlustbehaftet (Opus oder AAC, etwa 128 bis 160 kbit/s): Messungen über 16 kHz und
feine Transienten sind unzuverlässig, Tempo, Form, Bandbalance, Breite und Lautheitsverlauf nicht. Hochgeladene
Fassungen können beschleunigt, gepitcht oder Edits sein; die Beatport-Werte von Dok. 1 sind deshalb zugleich die
**Prüfung der Messwerkzeuge**: Tempo auf ±0,5 BPM, Tonart als Tonika; Abweichler fliegen raus.

**Was gemessen wird** (Phosphenes Werkzeuge, angepasst): Tempo und Downbeat; Kick in den Drum-Intros (die in Techno
länger und häufiger sind als in Psytrance): Grundton, Sweep, Decay, Klickband; Onset-Profil je Band im 16tel-Raster
(wo sitzen Hats, Claps, Perc); Bandbalance Sub/Bass/Mid/High und Schwerpunkt; Stereobreite je Terz; Lautheit
integriert und Short-Term, True Peak, Crest; Lautheitsminima (Reduktionen: Anzahl, Tiefe, Länge in Takten);
Abschnittsgrenzen (Foote-Novelty) und ihre Lage auf 16/32-Takt-Rastern; **Onset-Folgen je Band für den
Hypnose-Korridor** (Entropierate, PIR, Mikroveränderungsrate von Takt zu Takt).

### 13.5 Evaluation (Dok. 8.9)

Symbolisch je Track: LHL je Stimme, polyphone Syncopation, Gómez-Marín-Deskriptoren, Panteli-Periodizität, Euklidische
Gleichverteilung, Dichte-Deckel, Form- und Harmonik-Regeln, Hypnose-Korridor. Audio nach dem Render: dieselben Größen wie
13.4, gegen die Verteilungen der Referenzen je Profil. Ein Bericht (`Tools/eval_report.py`) stellt Soll und Ist nebeneinander,
wie Phosphenes `mix_audit.py`.

## 14. Phasen und Meilensteine

Aufwand in Arbeitstagen nach der Erfahrung mit Phosphene und Ephemeris; die Reihenfolge ist verbindlicher als die Zahlen.

| Phase | Inhalt | Prüfstein | Tage |
|---|---|---|---|
| **0 Gerüst** | Repo, CMake, Modulkopie (Ephemeris-Gerüst, Phosphene-Kit, Noctuary-Oversampling), Parametersystem, Clock, Partitur mit Block-Ops und Kurven, `tot_render`, Selbsttest-Skelett | `tot_render` gibt Stille mit Tempo-Karte aus; Vektortests grün | 1 bis 2 |
| **1 Ein Groove, der rollt** | Kick (drei Engines, zwei Schichten), Rumble frequenzgeteilt, Sub, CH-Offbeat und Rolling-Hat, Ducker, Rack mit Anker/Motion und Swing; erste Referenzmessung (nach Freigabe der Liste) | fünf Minuten eines Hypnotic-Loops, dessen Tief schiebt; Rumble-Test grün; Kick und Balance gegen die Referenzen; erste CPU-Zahlen | 3 bis 4 |
| **2 Die Pattern-Ebene** | alle Kit-Lanes, Matrizen von Dok. 8.2, Ghost-Ketten, Polymeter, Euklid, Displacement, Slipping, Trig-Conditions, Velocity/Timing, Fills, Mininotation; die Ping-Stimme | zehn Minuten mit Polymeter und Slipping; Rack-Tests grün; Onset-Profile gegen die Referenzen | 3 bis 4 |
| **3 Klang und Mix** | Bass-Synth mit Schaltungsfiltern, 303-Rolle, Dub-Chord mit Kette (Bandecho, Feder, Platte), Drone, Textur, Granular, globale Modulation, Kanalzüge, Multiband-Ducking, Bus, Master mit Clipper und TP-Limiter, Leveler, Tilt, Cut | Spektrum, Breite, Lautheit und Crest im Korridor der Referenzen; Hörvergleich je Profil | 4 bis 5 |
| **4 Komponist und Set** | Block-Grammatik mit Arc/Peak/Endless, Automation, Mikroereignisse, Harmonie, vier Profile mit Morph, Kandidaten und Hypnose-Korridor, Set mit Decks, Isolator, Bass-Swap, Tempodrift, Live-Rekomposition, Sperren, `.totset`, MIDI, Stems, DJ-Loops, Cues | ein Zwei-Stunden-Set aus einem Seed; Determinismus; Blend-Test; Evaluationsbericht | 5 bis 6 |
| **5 GUI** | Tabs, Eclipse-Ansicht, Arrange, Perform, Handbuch-Generator | Standalone und VST3 bedienbar; pluginval Strenge 10 | 4 bis 5 |
| **6 Quest** | NDK-Build, Qualitätsstufen, Performer-Oberfläche, Eclipse im Raum | Set läuft auf der Quest 2 unter 30 % eines Kerns, auch im Blend | 3 |
| **7 Qualität und Release** | Hörrunden, Nachkalibrierung, Cues, Installer, Handbuch, Social Preview | v1.0 | 3 bis 4 |

Nach Phase 1 gibt es den ersten hörbaren Prüfstein, nach Phase 4 ist das Produkt inhaltlich komplett.

## 15. Risiken

1. **Hypnose gegen Langeweile** (größtes Risiko): Ein Loop, der sich nur wiederholt, ist tot; einer, der zu viel tut,
   ist kein Techno mehr. Gegenmittel: der gemessene Korridor (2.9, 7.9), Mikroereignisse, früher Hörtest in Phase 1 und
   2, Sperren und Neuwürfeln.
2. **Das Tief:** Kick, Rumble und Sub können sich auslöschen oder verschmieren, und das hört man erst auf einer großen
   Anlage. Gegenmittel: der Rumble-Test als harte Prüfung, Messung der Summe statt der Teile, Referenzbalance.
3. **Lautheit ohne Plattmachen:** −10 LUFS verlangen einen echten Master (Clipper, Limiter), anders als bei den
   Geschwistern. Gegenmittel: Oversampling an allen Nichtlinearitäten, Crest-Korridor je Profil, Leveler.
4. **Referenzen aus YouTube:** verlustbehaftet, teils Edits, rechtlich nur zur Messung. Gegenmittel: nur Statistiken,
   Audio löschen, Prüfung gegen die Beatport-Werte, Liste vorher bestätigen.
5. **Unbelegte Pro-Step-Zahlen** (Dok. 8.10): Onset-Wahrscheinlichkeiten jenseits der Anker, Ghost-Velocity,
   Stab-Positionen, Rumble-Pegel, Reverb-Algorithmus sind [I]. Gegenmittel: Onset-Profile der Referenzen je Band,
   Hörrunden, Parameter offen im Style-Tab.
6. **Quest-Budget im Blend:** zwei Decks mit Rumble-Hall. Gegenmittel: Qualitätsstufen ab Phase 1, das abgehende Deck
   niedrig.
7. **Name:** vor dem Release prüfen, ob "Umbra" als Audio-Software schon vergeben ist. Geprüft: vergeben;
   am 28.09.2026 in Totality umbenannt.
8. **DJ-Software:** ob Cue-Chunks in WAV von Rekordbox, Traktor und Serato gelesen werden, ist ungeprüft; JSON-Cues sind
   die Rückfallebene.
9. **Rechtliches:** Künstlernamen nur in der Dokumentation, keine Fremd-Samples.

## 16. Entscheidungen des Nutzers (27.09.2026)

### 16.1 Getroffen

1. **Eigenes Projekt** in `G:\Tools\VRAudio\TechnoGenerator`, nicht in Noctuary integriert.
2. **Name:** Umbra; am 28.09.2026 umbenannt in **Totality** (Risiko 7).
3. **Stilbreite:** Hypnotic im Zentrum, dazu Ostgut, Dub und Raw/Peak als Profile, zwischen denen interpoliert wird;
   Hypnotic ist der Standard und wird zuerst kalibriert.
4. **Quest:** ja, der komplette Generator auf dem Gerät.
5. **Referenzen:** die im Dokument genannten Tracks, über YouTube bezogen; die Liste von 30 Titeln (Ergänzungen für
   Hypnotic und Dub) am 27.09.2026 freigegeben, das Audio bleibt außerhalb des Repos liegen ("Ja, Audio behalten").
6. **Bausteine** aller drei Geschwister nutzen, ausdrücklich auch Noctuarys (Abschnitt 4).
7. **Keine Agenten** bei der Arbeit an diesem Projekt.

### 16.2 Vorschläge, bestätigt am 27.09.2026 ("Ja, bitte ziehe das erst mal so durch")

1. **Keine Samples**, auch nicht für die 909-Hats (Metalltabelle im Code erzeugt) und die Textur; Phosphenes
   Field-Recordings nur als Option der Textur-Ebene, standardmäßig aus.
2. **Modulkopie statt Link**, Gerüst aus Ephemeris (Abschnitt 4).
3. **GUI und Handbuch Englisch**, Plan und Journal Deutsch.
4. **Keine lernenden Anteile** zu Beginn; später ein Ranker über Bewertungen (Phosphenes `Rating`).
5. **Im Host gilt das Host-Tempo**; die Set-Drift kommt über den MIDI-Export.
6. **Referenzliste** (13.4) mit den Ergänzungen für Hypnotic und Dub; Audio nach der Messung löschen. Das Laden selbst
   frage ich mit der konkreten Liste an, bevor es losgeht.
7. **Globaler Tempobereich 125 bis 136** statt 128 bis 136, damit Ostgut und Dub ihre Referenzen erreichen (2.3).

## 17. Literatur (Auswahl, je Baustein)

Die vollständige Quellenliste steht in Dok. 9; hier nur, was Totality über das Dokument hinaus oder an zentraler Stelle
benutzt.

- Abdallah, S.; Plumbley, M. (2009). Information dynamics: patterns of expectation and surprise in the perception of
  music. Connection Science 21(2), 89–117. (Predictive Information Rate)
- Parker, J.; D'Angelo, S. (2013). A Digital Model of the Buchla Lowpass-Gate. DAFx-13. (Ping)
- Werner, K. J.; Abel, J. S.; Smith, J. O. (2014). A Physically-Informed, Circuit-Bendable, Digital Model of the
  Roland TR-808 Bass Drum Circuit. DAFx-14. (Kick, Resonator-Engine)
- Werner, K. J.; Abel, J. S.; Smith, J. O. (2014). The TR-808 Cymbal: a Physically-Informed, Circuit-Bendable, Digital
  Model. ICMC. (Metall-Quelle)
- Reid, G. (2002). Synth Secrets, Teil 34 (Bassdrum-Synthese). Sound On Sound. (909-Topologie, wie in Dok. 3 verlinkt)
- Toussaint, G. (2005). The Euclidean Algorithm Generates Traditional Musical Rhythms. BRIDGES.
- Longuet-Higgins, H. C.; Lee, C. S. (1984). The Rhythmic Interpretation of Monophonic Music. Music Perception 1(4).
- Sioros, G.; Miron, M.; Davies, M.; Gouyon, F.; Madison, G. (2014). Syncopation Creates the Sensation of Groove in
  Synthesized Music Examples. Frontiers in Psychology 5.
- Witek, M. et al. (2014). Syncopation, Body-Movement and Pleasure in Groove Music. PLoS ONE 9(4).
- Butler, M. J. (2006). Unlocking the Groove: Rhythm, Meter, and Musical Design in Electronic Dance Music. Indiana UP.
- Eigenfeldt, A. (2013). GERP und GEDMAS, wie in Dok. 7 zitiert (eContact! 14.4; AIIDE 2013). (Onset-Matrizen,
  Markov-Ketten, Fitness)
- Grosz, P. et al. (2025). An Outline of the Narrative Grammar of Electronic Dance Music. Musicae Scientiae.
- Bittner, R. et al. (2017). Automatic Playlist Sequencing and Transitions. ISMIR.
- Foote, J. (2000). Automatic Audio Segmentation Using a Measure of Novelty. ICME.
- Dattorro, J. (1997). Effect Design, Part 1: Reverberator and Other Filters. JAES 45(9).
- Välimäki, V.; Parker, J.; Abel, J. S. (2010). Parametric Spring Reverberation Effect. JAES 58(7/8).
- Chowdhury, J. (2019). Real-Time Physical Modelling for Analog Tape Machines. DAFx.
- Huovilainen, A. (2004). Non-Linear Digital Implementation of the Moog Ladder Filter. DAFx.
- Flash, T.; Hogan, N. (1985). The Coordination of Arm Movements. Journal of Neuroscience 5(7). (Automationskurven)
- Giannoulis, D.; Massberg, M.; Reiss, J. D. (2012). Digital Dynamic Range Compressor Design. JAES.
- ITU-R BS.1770-4, EBU R 128 (Lautheit); ISO 226 (Kurven gleicher Lautstärke).

## Quellen der Recherche (27.09.2026)

Zur Prüfung der Ergänzung (2.11):
- Berghain, Deckenhöhe und Anlage: https://dansu.co.uk/blogs/news/10-clubs-with-iconic-sound-systems
- Berghain, Führer: https://techno-univers.com/en/blogs/guide/club-berghain
- Halle am Berghain, raumakustische Beratung: https://www.k5-akustik.de/en/projects/halle-am-berghain/
- Dubplates & Mastering, Neumann VMS 70: https://www.psaudio.com/blogs/copper/around-the-world-in-80-lathes-part-three
- Schneidestudios mit Maschinen: https://lathetrolls.com/viewtopic.php?t=1237&start=20
- Abdallah und Plumbley 2009: https://www.semanticscholar.org/paper/Information-dynamics:-patterns-of-expectation-and-Abdallah-Plumbley/846825312da286f833147835534869c585b895e7
