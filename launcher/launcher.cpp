// Launcher de re-wor: elige idioma, resolucion y nombre de usuario, muestra los DLC de la
// carpeta GH_DLC (compatibles y custom), ajusta los controles y abre el juego con esas
// opciones. Los ajustes se guardan en launcher.ini, junto al exe. Los textos del launcher
// cambian con el idioma elegido (el mismo que usa el juego).

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <gdiplus.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kGameExe[] = L"Guitar Hero Warriors of Rock.exe";
constexpr wchar_t kDlcFolder[] = L"GH_DLC";
constexpr wchar_t kIniFile[] = L"launcher.ini";
constexpr uint32_t kCompatibleTitles[] = {0x41560883, 0x41560840, 0x415607F7, 0x4156081A};
constexpr size_t kMaxNameLength = 15;

enum ControlId {
  kIdLanguage = 100,
  kIdResolution,
  kIdName,
  kIdLog,
  kIdOfficialList,
  kIdCustomList,
  kIdCustomStatus,
  kIdPlay,
  kIdOpenFolder,
  kIdRefresh,
  kIdExit,
  kIdTabs,
  kIdNativeGuitar,
  kIdTilt,
  kIdTiltLabel,
  kIdKeyboardEnable,
  kIdRestoreKeys,
  kIdAnim60,
  kIdPriorityList,
  kIdPriorityUp,
  kIdPriorityDown,
  kIdCacheHelp,
  kIdKeyBase,  // + 0..11: botones de cada tecla (debe ir al final)
};

// Pestanas de la barra superior.
enum Page { kPageSettings, kPageOfficial, kPageCustom, kPageControls, kPageKeyboard, kPageCount };

// ---------------------------------------------------------------------------
// Idiomas: los de Warriors of Rock, con su numero de idioma de Xbox (user_language).
// ---------------------------------------------------------------------------
struct Language {
  const wchar_t* code;  // en launcher.ini
  const wchar_t* name;  // en el selector, en su propio idioma
  int xbox_id;
};
constexpr Language kLanguages[] = {
    {L"en", L"English", 1}, {L"es", L"Español", 5}, {L"fr", L"Français", 4},
    {L"de", L"Deutsch", 3}, {L"it", L"Italiano", 6},
};
constexpr int kLanguageCount = int(sizeof(kLanguages) / sizeof(kLanguages[0]));

enum Text {
  kTabSettings, kTabOfficial, kTabCustom, kTabControls,
  kLabelLanguage, kLabelResolution, kRes720, kResX2, kLabelName, kNameHint, kSaveLog,
  kSettingsNote,
  kOfficialHeader, kCustomHeader,
  kColName, kColGame, kColSize, kColFile,
  kGameNotDlc, kGameWoR, kGameWorldTour, kGameOther, kGameIncompatible, kGameCache,
  kStatusNoCustomCache, kStatusNoCustom, kStatusMissingCache, kStatusManyCaches, kStatusCacheOk,
  kNativeGuitar, kNativeGuitarHint, kTiltLabel, kTiltValue, kTiltOff, kTiltHint, kKeysHelp,
  kOpenFolder, kRefresh, kExit, kPlay,
  kInvalidNameTitle, kInvalidName, kError, kGameNotFound, kCannotStart,
  kTabKeyboard, kKeyboardEnable, kKeyboardHint, kPressKey, kRestoreKeys,
  kKeyGreen, kKeyRed, kKeyYellow, kKeyBlue, kKeyOrange, kKeyStrumUp, kKeyStrumDown, kKeyWhammy,
  kKeyStar, kKeyStart, kKeyLeft, kKeyRight,
  kPriorityLabel, kPriorityUp, kPriorityDown, kDeviceGuitar, kDeviceKeyboard, kDevicePads,
  kAnim60, kAnim60Hint,
  kCacheHelpButton, kCacheHelpTitle, kCacheHelp, kMissingAssets,
  kTextCount
};

// Columnas: English, Espanol, Francais, Deutsch, Italiano.
const wchar_t* const kTexts[kTextCount][kLanguageCount] = {
    {L"Settings", L"Ajustes", L"Paramètres", L"Einstellungen", L"Impostazioni"},
    {L"Compatible DLC", L"DLC compatibles", L"DLC compatibles", L"Kompatible DLC",
     L"DLC compatibili"},
    {L"Custom DLC", L"DLC custom", L"DLC personnalisés", L"Eigene DLC", L"DLC personalizzati"},
    {L"Controls", L"Controles", L"Commandes", L"Steuerung", L"Controlli"},
    {L"Language:", L"Idioma:", L"Langue :", L"Sprache:", L"Lingua:"},
    {L"Resolution:", L"Resolución:", L"Résolution :", L"Auflösung:", L"Risoluzione:"},
    {L"720p (1280x720, normal)", L"720p (1280x720, normal)", L"720p (1280x720, normal)",
     L"720p (1280x720, normal)", L"720p (1280x720, normale)"},
    {L"x2 (2560x1440, sharper)", L"x2 (2560x1440, más nítido)", L"x2 (2560x1440, plus net)",
     L"x2 (2560x1440, schärfer)", L"x2 (2560x1440, più nitido)"},
    {L"User name:", L"Nombre de usuario:", L"Nom d'utilisateur :", L"Benutzername:",
     L"Nome utente:"},
    {L"Up to 15: letters without accents, numbers and spaces.",
     L"Máximo 15: letras sin acentos, números y espacios.",
     L"15 maximum : lettres sans accents, chiffres et espaces.",
     L"Höchstens 15: Buchstaben ohne Akzente, Ziffern und Leerzeichen.",
     L"Massimo 15: lettere senza accenti, numeri e spazi."},
    {L"Save log (to check problems)", L"Guardar log (para revisar problemas)",
     L"Enregistrer le journal (pour les problèmes)", L"Protokoll speichern (für Probleme)",
     L"Salva log (per controllare i problemi)"},
    {L"The language is used by the game and by this launcher.\n"
     L"x2 looks sharper but needs more from the graphics card.\n"
     L"The log goes to the game's \"logs\" folder (the previous one is kept too).",
     L"El idioma se usa en el juego y en este launcher.\n"
     L"x2 se ve más nítido pero exige más a la tarjeta de video.\n"
     L"El log queda en la carpeta \"logs\" del juego (también el de la vez anterior).",
     L"La langue est utilisée par le jeu et par ce launcher.\n"
     L"x2 est plus net mais demande plus à la carte graphique.\n"
     L"Le journal va dans le dossier \"logs\" du jeu (le précédent est conservé).",
     L"Die Sprache gilt für das Spiel und diesen Launcher.\n"
     L"x2 ist schärfer, fordert aber die Grafikkarte stärker.\n"
     L"Das Protokoll liegt im Ordner \"logs\" des Spiels (das vorherige bleibt erhalten).",
     L"La lingua è usata dal gioco e da questo launcher.\n"
     L"x2 è più nitido ma richiede di più alla scheda video.\n"
     L"Il log va nella cartella \"logs\" del gioco (anche quello precedente)."},
    {L"Official packages and exports from other Guitar Hero games (GH_DLC folder).",
     L"Paquetes oficiales y exports de otros Guitar Hero (carpeta GH_DLC).",
     L"Paquets officiels et exports d'autres Guitar Hero (dossier GH_DLC).",
     L"Offizielle Pakete und Exporte aus anderen Guitar Hero (Ordner GH_DLC).",
     L"Pacchetti ufficiali ed export da altri Guitar Hero (cartella GH_DLC)."},
    {L"Packs made by fans or with Onyx, and the custom song cache.",
     L"Packs hechos por fans o con Onyx, y el cache de canciones custom.",
     L"Packs faits par des fans ou avec Onyx, et le cache des chansons perso.",
     L"Von Fans oder mit Onyx erstellte Pakete und der Song-Cache.",
     L"Pack creati dai fan o con Onyx, e la cache delle canzoni personalizzate."},
    {L"Name", L"Nombre", L"Nom", L"Name", L"Nome"},
    {L"Game", L"Juego", L"Jeu", L"Spiel", L"Gioco"},
    {L"Size", L"Tamaño", L"Taille", L"Größe", L"Dimensione"},
    {L"File", L"Archivo", L"Fichier", L"Datei", L"File"},
    {L"Not DLC (type %08X)", L"No es DLC (tipo %08X)", L"Pas un DLC (type %08X)",
     L"Kein DLC (Typ %08X)", L"Non è un DLC (tipo %08X)"},
    {L"Warriors of Rock", L"Warriors of Rock", L"Warriors of Rock", L"Warriors of Rock",
     L"Warriors of Rock"},
    {L"World Tour / exports", L"World Tour / exports", L"World Tour / exports",
     L"World Tour / Exporte", L"World Tour / export"},
    {L"Other Guitar Hero (%08X)", L"Otro Guitar Hero (%08X)", L"Autre Guitar Hero (%08X)",
     L"Anderes Guitar Hero (%08X)", L"Altro Guitar Hero (%08X)"},
    {L"NOT compatible (game %08X)", L"NO compatible (juego %08X)",
     L"NON compatible (jeu %08X)", L"NICHT kompatibel (Spiel %08X)",
     L"NON compatibile (gioco %08X)"},
    {L"Custom song cache", L"Cache de canciones custom", L"Cache des chansons perso",
     L"Cache für eigene Songs", L"Cache delle canzoni personalizzate"},
    {L"No custom DLC; the cache is not needed.", L"No hay DLC custom; el cache no hace falta.",
     L"Aucun DLC personnalisé ; le cache n'est pas nécessaire.",
     L"Keine eigenen DLC; der Cache wird nicht gebraucht.",
     L"Nessun DLC personalizzato; la cache non serve."},
    {L"No custom DLC.", L"No hay DLC custom.", L"Aucun DLC personnalisé.", L"Keine eigenen DLC.",
     L"Nessun DLC personalizzato."},
    {L"WARNING: there is custom DLC but the cache is missing. Custom packs can hide official ones.",
     L"AVISO: hay DLC custom pero falta el cache. Los custom pueden tapar a los oficiales.",
     L"ATTENTION : DLC personnalisés sans cache. Ils peuvent masquer les officiels.",
     L"WARNUNG: eigene DLC ohne Cache. Sie können offizielle DLC verdecken.",
     L"ATTENZIONE: DLC personalizzati senza cache. Possono nascondere quelli ufficiali."},
    {L"WARNING: there is more than one cache. Keep only one (the one with all the DLC).",
     L"AVISO: hay más de un cache. Deja solo uno (el que incluya todos los DLC).",
     L"ATTENTION : plusieurs caches. N'en garde qu'un (celui avec tous les DLC).",
     L"WARNUNG: mehr als ein Cache. Behalte nur einen (den mit allen DLC).",
     L"ATTENZIONE: più di una cache. Tienine solo una (quella con tutti i DLC)."},
    {L"Cache found. Remember to rebuild it (in Onyx) every time you add DLC.",
     L"Cache encontrado. Recuerda rehacerlo (en Onyx) cada vez que agregues DLC.",
     L"Cache trouvé. Pense à le refaire (dans Onyx) à chaque ajout de DLC.",
     L"Cache gefunden. Erstelle ihn (in Onyx) neu, wenn du DLC hinzufügst.",
     L"Cache trovata. Ricorda di rifarla (in Onyx) ogni volta che aggiungi DLC."},
    {L"Read Xbox One guitars directly", L"Leer guitarras de Xbox One directamente",
     L"Lire directement les guitares Xbox One", L"Xbox-One-Gitarren direkt lesen",
     L"Leggi direttamente le chitarre Xbox One"},
    {L"PDP Riffmaster, PDP Jaguar and Mad Catz Stratocaster, without extra programs. Turn it\n"
     L"off if you use RB4InstrumentMapper or another program that creates a virtual Xbox 360 pad.",
     L"PDP Riffmaster, PDP Jaguar y Mad Catz Stratocaster, sin programas extra. Desactívalo\n"
     L"si usas RB4InstrumentMapper u otro programa que cree un mando de Xbox 360 virtual.",
     L"PDP Riffmaster, PDP Jaguar et Mad Catz Stratocaster, sans autre programme. Désactive-le\n"
     L"si tu utilises RB4InstrumentMapper ou un programme qui crée une manette Xbox 360 virtuelle.",
     L"PDP Riffmaster, PDP Jaguar und Mad Catz Stratocaster, ohne Zusatzprogramme. Ausschalten,\n"
     L"wenn du RB4InstrumentMapper o. Ä. nutzt, das einen virtuellen Xbox-360-Controller erstellt.",
     L"PDP Riffmaster, PDP Jaguar e Mad Catz Stratocaster, senza programmi extra. Disattivalo\n"
     L"se usi RB4InstrumentMapper o un programma che crea un controller Xbox 360 virtuale."},
    {L"Tilt needed to activate Star Power:", L"Inclinación (tilt) para activar el Star Power:",
     L"Inclinaison nécessaire pour le Star Power :", L"Neigung für Star Power:",
     L"Inclinazione per attivare lo Star Power:"},
    {L"%d %% tilt", L"%d %% de inclinación", L"%d %% d'inclinaison", L"%d %% Neigung",
     L"%d %% di inclinazione"},
    {L"0 %%: no change (original game)", L"0 %%: sin ajuste (juego original)",
     L"0 %% : aucun réglage (jeu original)", L"0 %%: keine Änderung (Originalspiel)",
     L"0 %%: nessuna modifica (gioco originale)"},
    {L"Lower = lifting the neck a little is enough. 0 = like the original game.",
     L"Menos porcentaje = basta con levantar un poco el mástil. 0 = como el juego original.",
     L"Moins = il suffit de lever un peu le manche. 0 = comme le jeu original.",
     L"Weniger = den Hals etwas anheben reicht. 0 = wie im Originalspiel.",
     L"Meno = basta alzare un po' il manico. 0 = come il gioco originale."},
    {L"Useful keys during the game:\n   F3   show or hide FPS\n   F4   SDK technical settings",
     L"Teclas útiles durante el juego:\n   F3   muestra u oculta los FPS\n"
     L"   F4   ajustes técnicos del SDK",
     L"Touches utiles en jeu :\n   F3   afficher ou masquer les FPS\n"
     L"   F4   réglages techniques du SDK",
     L"Nützliche Tasten im Spiel:\n   F3   FPS ein- oder ausblenden\n"
     L"   F4   technische SDK-Einstellungen",
     L"Tasti utili durante il gioco:\n   F3   mostra o nasconde gli FPS\n"
     L"   F4   impostazioni tecniche dell'SDK"},
    {L"Open DLC folder", L"Abrir carpeta de DLC", L"Ouvrir le dossier DLC", L"DLC-Ordner öffnen",
     L"Apri cartella DLC"},
    {L"Refresh list", L"Actualizar lista", L"Actualiser", L"Liste aktualisieren",
     L"Aggiorna elenco"},
    {L"Exit", L"Salir", L"Quitter", L"Beenden", L"Esci"},
    {L"Play", L"Jugar", L"Jouer", L"Spielen", L"Gioca"},
    {L"Invalid name", L"Nombre no válido", L"Nom non valide", L"Ungültiger Name",
     L"Nome non valido"},
    {L"The user name must have 1 to 15 characters: letters without accents, numbers and spaces.",
     L"El nombre de usuario debe tener entre 1 y 15 caracteres: letras sin acentos, números y "
     L"espacios.",
     L"Le nom doit avoir de 1 à 15 caractères : lettres sans accents, chiffres et espaces.",
     L"Der Name muss 1 bis 15 Zeichen haben: Buchstaben ohne Akzente, Ziffern und Leerzeichen.",
     L"Il nome deve avere da 1 a 15 caratteri: lettere senza accenti, numeri e spazi."},
    {L"Error", L"Error", L"Erreur", L"Fehler", L"Errore"},
    {L"The game was not found:\n", L"No se encuentra el juego:\n", L"Jeu introuvable :\n",
     L"Spiel nicht gefunden:\n", L"Gioco non trovato:\n"},
    {L"The game could not be started.", L"No se pudo abrir el juego.",
     L"Impossible de lancer le jeu.", L"Das Spiel konnte nicht gestartet werden.",
     L"Impossibile avviare il gioco."},
    {L"Keyboard mapping", L"Mapeado de teclado", L"Clavier", L"Tastatur", L"Mappatura tastiera"},
    {L"Use the keyboard as a guitar (player 1)", L"Usar el teclado como guitarra (jugador 1)",
     L"Utiliser le clavier comme guitare (joueur 1)",
     L"Tastatur als Gitarre verwenden (Spieler 1)",
     L"Usa la tastiera come chitarra (giocatore 1)"},
    {L"Click a button and press the new key (Esc cancels). Works with or without a guitar.",
     L"Haz clic en un botón y aprieta la tecla nueva (Esc cancela). Funciona con o sin guitarra.",
     L"Clique sur un bouton et appuie sur la nouvelle touche (Échap annule). Avec ou sans guitare.",
     L"Klicke auf eine Schaltfläche und drücke die neue Taste (Esc bricht ab). Mit oder ohne Gitarre.",
     L"Fai clic su un pulsante e premi il nuovo tasto (Esc annulla). Con o senza chitarra."},
    {L"Press a key...", L"Aprieta una tecla...", L"Appuie sur une touche...",
     L"Taste drücken...", L"Premi un tasto..."},
    {L"Restore defaults", L"Restaurar predeterminado", L"Par défaut", L"Standard wiederherstellen",
     L"Ripristina predefiniti"},
    {L"Green fret", L"Traste verde", L"Frette verte", L"Grüner Bund", L"Tasto verde"},
    {L"Red fret", L"Traste rojo", L"Frette rouge", L"Roter Bund", L"Tasto rosso"},
    {L"Yellow fret", L"Traste amarillo", L"Frette jaune", L"Gelber Bund", L"Tasto giallo"},
    {L"Blue fret", L"Traste azul", L"Frette bleue", L"Blauer Bund", L"Tasto blu"},
    {L"Orange fret", L"Traste naranja", L"Frette orange", L"Oranger Bund", L"Tasto arancione"},
    {L"Strum up", L"Rasgueo arriba", L"Gratter haut", L"Anschlag hoch", L"Strum su"},
    {L"Strum down", L"Rasgueo abajo", L"Gratter bas", L"Anschlag runter", L"Strum giù"},
    {L"Whammy", L"Whammy", L"Whammy", L"Whammy", L"Whammy"},
    {L"Star Power", L"Energía estrella", L"Star Power", L"Star Power", L"Star Power"},
    {L"Start", L"Start", L"Start", L"Start", L"Start"},
    {L"Left (menus)", L"Izquierda (menús)", L"Gauche (menus)", L"Links (Menüs)",
     L"Sinistra (menu)"},
    {L"Right (menus)", L"Derecha (menús)", L"Droite (menus)", L"Rechts (Menüs)",
     L"Destra (menu)"},
    {L"Player order (the first one connected is player 1):",
     L"Orden de los jugadores (el primero conectado es el jugador 1):",
     L"Ordre des joueurs (le premier connecté est le joueur 1) :",
     L"Spielerreihenfolge (das erste verbundene Gerät ist Spieler 1):",
     L"Ordine dei giocatori (il primo collegato è il giocatore 1):"},
    {L"Move up", L"Subir", L"Monter", L"Nach oben", L"Su"},
    {L"Move down", L"Bajar", L"Descendre", L"Nach unten", L"Giù"},
    {L"Xbox One guitar (Riffmaster, Jaguar, Stratocaster)",
     L"Guitarra de Xbox One (Riffmaster, Jaguar, Stratocaster)",
     L"Guitare Xbox One (Riffmaster, Jaguar, Stratocaster)",
     L"Xbox-One-Gitarre (Riffmaster, Jaguar, Stratocaster)",
     L"Chitarra Xbox One (Riffmaster, Jaguar, Stratocaster)"},
    {L"Keyboard", L"Teclado", L"Clavier", L"Tastatur", L"Tastiera"},
    {L"Xbox 360 controllers and guitars", L"Mandos y guitarras de Xbox 360",
     L"Manettes et guitares Xbox 360", L"Xbox-360-Controller und -Gitarren",
     L"Controller e chitarre Xbox 360"},
    {L"Animations at 60 FPS (smoother characters and stage)",
     L"Animaciones a 60 FPS (personajes y escenario más fluidos)",
     L"Animations à 60 FPS (personnages et décor plus fluides)",
     L"Animationen mit 60 FPS (flüssigere Figuren und Bühne)",
     L"Animazioni a 60 FPS (personaggi e scenario più fluidi)"},
    {L"The original game animates at 30. Uses more CPU.",
     L"El juego original anima a 30. Usa más CPU.",
     L"Le jeu original anime à 30. Utilise plus de processeur.",
     L"Das Originalspiel animiert mit 30. Braucht mehr CPU.",
     L"Il gioco originale anima a 30. Usa più CPU."},
    {L"How do I create the cache?", L"¿Cómo crear el cache?", L"Comment créer le cache ?",
     L"Wie erstelle ich den Cache?", L"Come creare la cache?"},
    {L"Custom song cache", L"Cache de canciones custom", L"Cache des chansons perso",
     L"Cache für eigene Songs", L"Cache delle canzoni personalizzate"},
    {L"For DLC and custom songs (CDLC) to work properly, create a song cache with Onyx "
     L"Music Game Toolkit (free: github.com/mtolly/onyx/releases).\n\n"
     L"STEPS:\n"
     L"1. Put all your packages (official DLC and custom) in the GH_DLC folder.\n"
     L"     Use the \"Open DLC folder\" button.\n"
     L"2. Open Onyx and press \"Other tools\".\n"
     L"3. Go to the \"GH:WoR cache\" tab.\n"
     L"4. Add ALL the packages from GH_DLC, official and custom.\n"
     L"     If you leave an official Warriors of Rock DLC out, it will not appear in the game.\n"
     L"5. Press \"Create GH:WoR cache (360)\".\n"
     L"6. Save the file in GH_DLC\\Cache (for example as ghwor_custom_cache).\n"
     L"7. Leave only ONE cache in GH_DLC: move the old one to a folder outside GH_DLC.\n"
     L"8. Press \"Refresh list\": the cache appears in this table.\n\n"
     L"IMPORTANT:\n"
     L"- Rebuild the cache every time you add or remove DLC.\n"
     L"- Do not delete the packages: the cache is only the song list, the songs are still in "
     L"the packages.\n"
     L"- With only official DLC (no custom songs), the cache is not needed.\n"
     L"- Back up your saves before adding many new songs.",
     L"Para que los DLC y las canciones custom (CDLC) funcionen bien, crea un cache de "
     L"canciones con Onyx Music Game Toolkit (gratis: github.com/mtolly/onyx/releases).\n\n"
     L"PASOS:\n"
     L"1. Pon todos tus paquetes (DLC oficiales y custom) en la carpeta GH_DLC.\n"
     L"     Usa el botón \"Abrir carpeta de DLC\".\n"
     L"2. Abre Onyx y presiona \"Other tools\".\n"
     L"3. Ve a la pestaña \"GH:WoR cache\".\n"
     L"4. Agrega TODOS los paquetes de GH_DLC, oficiales y custom.\n"
     L"     Si dejas fuera un DLC oficial de Warriors of Rock, no aparecerá en el juego.\n"
     L"5. Presiona \"Create GH:WoR cache (360)\".\n"
     L"6. Guarda el archivo en GH_DLC\\Cache (por ejemplo como ghwor_custom_cache).\n"
     L"7. Deja UN solo cache en GH_DLC: mueve el viejo a una carpeta fuera de GH_DLC.\n"
     L"8. Presiona \"Actualizar lista\": el cache aparece en esta tabla.\n\n"
     L"IMPORTANTE:\n"
     L"- Rehaz el cache cada vez que agregues o quites DLC.\n"
     L"- No borres los paquetes: el cache es solo la lista de canciones, las canciones siguen "
     L"en los paquetes.\n"
     L"- Si solo tienes DLC oficiales (sin custom), el cache no hace falta.\n"
     L"- Haz una copia de tus partidas antes de agregar muchas canciones nuevas.",
     L"Pour que les DLC et les chansons perso (CDLC) fonctionnent bien, crée un cache de "
     L"chansons avec Onyx Music Game Toolkit (gratuit : github.com/mtolly/onyx/releases).\n\n"
     L"ÉTAPES :\n"
     L"1. Mets tous tes paquets (DLC officiels et perso) dans le dossier GH_DLC.\n"
     L"     Utilise le bouton \"Ouvrir le dossier DLC\".\n"
     L"2. Ouvre Onyx et clique sur \"Other tools\".\n"
     L"3. Va dans l'onglet \"GH:WoR cache\".\n"
     L"4. Ajoute TOUS les paquets de GH_DLC, officiels et perso.\n"
     L"     Si tu oublies un DLC officiel de Warriors of Rock, il n'apparaîtra pas dans le jeu.\n"
     L"5. Clique sur \"Create GH:WoR cache (360)\".\n"
     L"6. Enregistre le fichier dans GH_DLC\\Cache (par exemple ghwor_custom_cache).\n"
     L"7. Ne garde qu'UN seul cache dans GH_DLC : déplace l'ancien hors de GH_DLC.\n"
     L"8. Clique sur \"Actualiser\" : le cache apparaît dans ce tableau.\n\n"
     L"IMPORTANT :\n"
     L"- Refais le cache à chaque ajout ou retrait de DLC.\n"
     L"- Ne supprime pas les paquets : le cache n'est que la liste, les chansons restent dans "
     L"les paquets.\n"
     L"- Avec seulement des DLC officiels (sans perso), le cache n'est pas nécessaire.\n"
     L"- Sauvegarde tes parties avant d'ajouter beaucoup de nouvelles chansons.",
     L"Damit DLC und eigene Songs (CDLC) richtig funktionieren, erstelle einen Song-Cache mit "
     L"Onyx Music Game Toolkit (kostenlos: github.com/mtolly/onyx/releases).\n\n"
     L"SCHRITTE:\n"
     L"1. Lege alle Pakete (offizielle DLC und eigene) in den Ordner GH_DLC.\n"
     L"     Benutze den Knopf \"DLC-Ordner öffnen\".\n"
     L"2. Öffne Onyx und klicke auf \"Other tools\".\n"
     L"3. Gehe zum Reiter \"GH:WoR cache\".\n"
     L"4. Füge ALLE Pakete aus GH_DLC hinzu, offizielle und eigene.\n"
     L"     Fehlt ein offizieller Warriors-of-Rock-DLC, erscheint er nicht im Spiel.\n"
     L"5. Klicke auf \"Create GH:WoR cache (360)\".\n"
     L"6. Speichere die Datei in GH_DLC\\Cache (z. B. als ghwor_custom_cache).\n"
     L"7. Behalte nur EINEN Cache in GH_DLC: verschiebe den alten aus GH_DLC heraus.\n"
     L"8. Klicke auf \"Liste aktualisieren\": der Cache erscheint in dieser Tabelle.\n\n"
     L"WICHTIG:\n"
     L"- Erstelle den Cache neu, wenn du DLC hinzufügst oder entfernst.\n"
     L"- Lösche die Pakete nicht: der Cache ist nur die Songliste, die Songs bleiben in den "
     L"Paketen.\n"
     L"- Nur mit offiziellen DLC (ohne eigene Songs) wird kein Cache gebraucht.\n"
     L"- Sichere deine Spielstände, bevor du viele neue Songs hinzufügst.",
     L"Perché i DLC e le canzoni personalizzate (CDLC) funzionino bene, crea una cache delle "
     L"canzoni con Onyx Music Game Toolkit (gratis: github.com/mtolly/onyx/releases).\n\n"
     L"PASSI:\n"
     L"1. Metti tutti i pacchetti (DLC ufficiali e personalizzati) nella cartella GH_DLC.\n"
     L"     Usa il pulsante \"Apri cartella DLC\".\n"
     L"2. Apri Onyx e premi \"Other tools\".\n"
     L"3. Vai alla scheda \"GH:WoR cache\".\n"
     L"4. Aggiungi TUTTI i pacchetti di GH_DLC, ufficiali e personalizzati.\n"
     L"     Se escludi un DLC ufficiale di Warriors of Rock, non apparirà nel gioco.\n"
     L"5. Premi \"Create GH:WoR cache (360)\".\n"
     L"6. Salva il file in GH_DLC\\Cache (ad esempio come ghwor_custom_cache).\n"
     L"7. Lascia UNA sola cache in GH_DLC: sposta quella vecchia fuori da GH_DLC.\n"
     L"8. Premi \"Aggiorna elenco\": la cache appare in questa tabella.\n\n"
     L"IMPORTANTE:\n"
     L"- Rifai la cache ogni volta che aggiungi o togli DLC.\n"
     L"- Non cancellare i pacchetti: la cache è solo l'elenco, le canzoni restano nei "
     L"pacchetti.\n"
     L"- Con soli DLC ufficiali (senza personalizzati) la cache non serve.\n"
     L"- Fai una copia dei salvataggi prima di aggiungere molte canzoni nuove."},
    {L"The game files are missing (assets\\default.xex was not found).\n\n"
     L"Extract the contents of YOUR OWN Xbox 360 disc of Guitar Hero: Warriors of Rock (for "
     L"example with extract-xiso) into the \"assets\" folder next to the launcher. It must "
     L"contain default.xex and the data folder.",
     L"Faltan los archivos del juego (no se encontró assets\\default.xex).\n\n"
     L"Extrae el contenido de TU PROPIO disco de Xbox 360 de Guitar Hero: Warriors of Rock (por "
     L"ejemplo con extract-xiso) en la carpeta \"assets\" junto al launcher. Debe contener "
     L"default.xex y la carpeta data.",
     L"Les fichiers du jeu sont absents (assets\\default.xex introuvable).\n\n"
     L"Extrais le contenu de TON PROPRE disque Xbox 360 de Guitar Hero: Warriors of Rock (par "
     L"exemple avec extract-xiso) dans le dossier \"assets\" à côté du launcher. Il doit "
     L"contenir default.xex et le dossier data.",
     L"Die Spieldateien fehlen (assets\\default.xex nicht gefunden).\n\n"
     L"Entpacke den Inhalt DEINER EIGENEN Xbox-360-Disc von Guitar Hero: Warriors of Rock (z. B. "
     L"mit extract-xiso) in den Ordner \"assets\" neben dem Launcher. Er muss default.xex und "
     L"den Ordner data enthalten.",
     L"Mancano i file del gioco (assets\\default.xex non trovato).\n\n"
     L"Estrai il contenuto del TUO disco Xbox 360 di Guitar Hero: Warriors of Rock (ad esempio "
     L"con extract-xiso) nella cartella \"assets\" accanto al launcher. Deve contenere "
     L"default.xex e la cartella data."},
};

// Orden de los jugadores (input_priority del SDK).
constexpr const wchar_t* kDeviceIds[3] = {L"guitarra", L"teclado", L"mandos"};
int g_priority[3] = {0, 1, 2};  // indices en kDeviceIds, en orden
HWND g_priority_list;

const wchar_t* T(Text text);
std::wstring ReadIni(const wchar_t* key, const wchar_t* fallback);

// Teclas de la guitarra (codigos VK de Windows), en el orden de keyboard_map del SDK.
constexpr int kKeyCount = 12;
constexpr int kDefaultKeys[kKeyCount] = {'1', '2', '3', '4', '5', VK_UP, VK_DOWN,
                                          VK_CONTROL, VK_SHIFT, VK_RETURN, VK_LEFT, VK_RIGHT};
int g_keys[kKeyCount];
HWND g_key_buttons[kKeyCount];
HWND g_keyboard_enable;
int g_capture = -1;  // tecla que se esta cambiando (-1 = ninguna)

std::wstring KeyName(int vk) {
  switch (vk) {
    case VK_UP: return L"↑";
    case VK_DOWN: return L"↓";
    case VK_LEFT: return L"←";
    case VK_RIGHT: return L"→";
    case VK_CONTROL: return L"Ctrl";
    case VK_SHIFT: return L"Shift";
    case VK_MENU: return L"Alt";
    case VK_RETURN: return L"Enter";
    case VK_SPACE: return L"Espacio / Space";
    case VK_BACK: return L"Backspace";
    case VK_TAB: return L"Tab";
  }
  if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
    return std::wstring(1, wchar_t(vk));
  }
  if (vk >= VK_F1 && vk <= VK_F24) {
    return L"F" + std::to_wstring(vk - VK_F1 + 1);
  }
  wchar_t name[64] = {};
  GetKeyNameTextW(LONG(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)) << 16, name, 64);
  return name[0] ? name : L"VK " + std::to_wstring(vk);
}

void LoadKeys() {
  std::wstring text = ReadIni(L"teclas", L"");
  for (int i = 0; i < kKeyCount; ++i) {
    g_keys[i] = kDefaultKeys[i];
  }
  size_t start = 0;
  for (int i = 0; i < kKeyCount && start < text.size(); ++i) {
    size_t end = text.find(L',', start);
    int vk = _wtoi(text.substr(start, end - start).c_str());
    if (vk > 0 && vk < 256) {
      g_keys[i] = vk;
    }
    start = end == std::wstring::npos ? text.size() : end + 1;
  }
}

std::wstring KeysText() {
  std::wstring text;
  for (int i = 0; i < kKeyCount; ++i) {
    text += (i ? L"," : L"") + std::to_wstring(g_keys[i]);
  }
  return text;
}

void LoadPriority() {
  std::wstring text = ReadIni(L"prioridad", L"guitarra,teclado,mandos");
  int order[3], count = 0;
  bool used[3] = {};
  size_t start = 0;
  while (start <= text.size() && count < 3) {
    size_t end = text.find(L',', start);
    std::wstring id = text.substr(start, end == std::wstring::npos ? std::wstring::npos
                                                                   : end - start);
    for (int i = 0; i < 3; ++i) {
      if (id == kDeviceIds[i] && !used[i]) {
        used[i] = true;
        order[count++] = i;
      }
    }
    if (end == std::wstring::npos) {
      break;
    }
    start = end + 1;
  }
  for (int i = 0; i < 3; ++i) {  // los que falten, al final
    if (!used[i]) {
      order[count++] = i;
    }
  }
  for (int i = 0; i < 3; ++i) {
    g_priority[i] = order[i];
  }
}

std::wstring PriorityText() {
  std::wstring text;
  for (int i = 0; i < 3; ++i) {
    text += (i ? L"," : L"") + std::wstring(kDeviceIds[g_priority[i]]);
  }
  return text;
}

void UpdatePriorityList(int selected) {
  const Text names[3] = {kDeviceGuitar, kDeviceKeyboard, kDevicePads};
  SendMessageW(g_priority_list, LB_RESETCONTENT, 0, 0);
  for (int i = 0; i < 3; ++i) {
    std::wstring line = std::to_wstring(i + 1) + L".  " + T(names[g_priority[i]]);
    SendMessageW(g_priority_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
  }
  SendMessageW(g_priority_list, LB_SETCURSEL, selected, 0);
}

void MovePriority(int direction) {
  int selected = int(SendMessageW(g_priority_list, LB_GETCURSEL, 0, 0));
  int target = selected + direction;
  if (selected < 0 || target < 0 || target >= 3) {
    return;
  }
  std::swap(g_priority[selected], g_priority[target]);
  UpdatePriorityList(target);
}

void UpdateKeyButtons() {
  for (int i = 0; i < kKeyCount; ++i) {
    SetWindowTextW(g_key_buttons[i], i == g_capture ? T(kPressKey) : KeyName(g_keys[i]).c_str());
  }
}

int g_language = 1;  // indice en kLanguages (Espanol por defecto)
const wchar_t* T(Text text) { return kTexts[text][g_language]; }

struct DlcPackage {
  std::wstring display_name;
  std::wstring relative_path;
  uint32_t title_id = 0;
  uint32_t content_type = 0;
  uint64_t size = 0;
  bool official = false;
  bool cache = false;
};

HWND g_window, g_language_box, g_resolution, g_name, g_log, g_official_list, g_custom_list,
    g_custom_status;
HWND g_tabs, g_native_guitar, g_tilt, g_tilt_label, g_title, g_anim60;
HFONT g_font, g_bold_font;
fs::path g_dir;
std::vector<HWND> g_pages[kPageCount];  // controles de cada pestana
int g_building_page = -1;  // pestana a la que se agregan los controles nuevos (-1 = siempre visibles)
bool g_status_warning = false;

// ---------------------------------------------------------------------------
// Fondo: imagen recortada para cubrir la ventana y oscurecida para que se lea el texto.
// Usa fondo_launcher.png junto al exe si existe; si no, un fondo dibujado (sin arte del juego).
// Los textos y casillas se pintan con un pincel de esa misma imagen (alineado a su
// posicion), asi parecen transparentes y se redibujan bien al cambiar el texto.
// ---------------------------------------------------------------------------
constexpr wchar_t kBackgroundFile[] = L"fondo_launcher.png";
constexpr COLORREF kTextColor = RGB(240, 230, 215);
constexpr COLORREF kTitleColor = RGB(255, 196, 90);
constexpr COLORREF kListBack = RGB(28, 18, 14);
HBITMAP g_background;
HBRUSH g_background_brush;

Gdiplus::Bitmap* LoadBackgroundImage() {
  fs::path file = g_dir / kBackgroundFile;
  std::error_code ec;
  if (fs::exists(file, ec)) {
    auto* image = Gdiplus::Bitmap::FromFile(file.c_str());
    if (image && image->GetLastStatus() == Gdiplus::Ok) {
      return image;
    }
    delete image;
  }
  return nullptr;
}

// Fondo propio (sin arte del juego): degradado oscuro con haces de luz de escenario.
void DrawDefaultBackground(Gdiplus::Graphics& graphics, int width, int height) {
  Gdiplus::LinearGradientBrush base(Gdiplus::Point(0, 0), Gdiplus::Point(0, height),
                                    Gdiplus::Color(255, 48, 14, 10), Gdiplus::Color(255, 8, 5, 6));
  graphics.FillRectangle(&base, 0, 0, width, height);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  // Cada haz sale de un foco arriba y se abre hacia abajo, cada vez mas tenue.
  struct Beam { float x, spread, tilt; BYTE alpha; };
  const Beam beams[] = {{0.12f, 0.16f, 0.22f, 70}, {0.38f, 0.12f, 0.05f, 45},
                        {0.66f, 0.14f, -0.08f, 50}, {0.90f, 0.18f, -0.25f, 70}};
  for (const Beam& beam : beams) {
    float top = beam.x * width, bottom = top + beam.tilt * width;
    Gdiplus::PointF points[] = {{top - 6, -4}, {top + 6, -4},
                                {bottom + beam.spread * width, float(height)},
                                {bottom - beam.spread * width, float(height)}};
    Gdiplus::LinearGradientBrush light(Gdiplus::Point(0, 0), Gdiplus::Point(0, height),
                                       Gdiplus::Color(beam.alpha, 255, 170, 80),
                                       Gdiplus::Color(0, 255, 120, 40));
    graphics.FillPolygon(&light, points, 4);
  }
  // Brillo de escenario abajo y bordes oscurecidos.
  Gdiplus::GraphicsPath glow;
  glow.AddEllipse(-width / 4, height * 3 / 4, width * 3 / 2, height / 2);
  Gdiplus::PathGradientBrush glow_brush(&glow);
  glow_brush.SetCenterColor(Gdiplus::Color(60, 255, 90, 30));
  Gdiplus::Color edge(0, 255, 90, 30);
  int count = 1;
  glow_brush.SetSurroundColors(&edge, &count);
  graphics.FillPath(&glow_brush, &glow);
  Gdiplus::GraphicsPath vignette;
  vignette.AddEllipse(-width / 3, -height / 3, width * 5 / 3, height * 5 / 3);
  Gdiplus::PathGradientBrush vignette_brush(&vignette);
  vignette_brush.SetCenterColor(Gdiplus::Color(0, 0, 0, 0));
  Gdiplus::Color dark(170, 0, 0, 0);
  vignette_brush.SetSurroundColors(&dark, &count);
  graphics.FillRectangle(&vignette_brush, 0, 0, width, height);
}

void CreateBackground(int width, int height) {
  Gdiplus::Bitmap* image = LoadBackgroundImage();
  Gdiplus::Bitmap canvas(width, height, PixelFormat32bppRGB);
  Gdiplus::Graphics graphics(&canvas);
  if (image) {
    // Recorte tipo "cubrir": la imagen llena la ventana sin deformarse.
    double scale = std::max(double(width) / image->GetWidth(), double(height) / image->GetHeight());
    int draw_width = int(image->GetWidth() * scale), draw_height = int(image->GetHeight() * scale);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(image, (width - draw_width) / 2, (height - draw_height) / 2, draw_width,
                       draw_height);
    delete image;
    Gdiplus::SolidBrush shade(Gdiplus::Color(150, 0, 0, 0));
    graphics.FillRectangle(&shade, 0, 0, width, height);
  } else {
    DrawDefaultBackground(graphics, width, height);
  }
  canvas.GetHBITMAP(Gdiplus::Color(0, 0, 0), &g_background);
  g_background_brush = CreatePatternBrush(g_background);
}

void PaintBackground(HDC dc) {
  RECT client;
  GetClientRect(g_window, &client);
  HDC memory = CreateCompatibleDC(dc);
  HGDIOBJ old = SelectObject(memory, g_background);
  BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
  SelectObject(memory, old);
  DeleteDC(memory);
}

// Pincel del fondo alineado con la posicion del control dentro de la ventana.
HBRUSH BackgroundBrushFor(HWND control, HDC dc) {
  RECT rect;
  GetWindowRect(control, &rect);
  MapWindowPoints(nullptr, g_window, reinterpret_cast<POINT*>(&rect), 2);
  SetBrushOrgEx(dc, -rect.left, -rect.top, nullptr);
  return g_background_brush;
}

fs::path IniPath() { return g_dir / kIniFile; }

std::wstring ReadIni(const wchar_t* key, const wchar_t* fallback) {
  wchar_t buffer[256];
  GetPrivateProfileStringW(L"Launcher", key, fallback, buffer, 256, IniPath().c_str());
  return buffer;
}

void WriteIni(const wchar_t* key, const std::wstring& value) {
  WritePrivateProfileStringW(L"Launcher", key, value.c_str(), IniPath().c_str());
}

int LanguageFromIni() {
  std::wstring code = ReadIni(L"idioma", L"es");
  for (int i = 0; i < kLanguageCount; ++i) {
    if (code == kLanguages[i].code) {
      return i;
    }
  }
  return 1;
}

// tilt_threshold del SDK (0..32767) mostrado como porcentaje de inclinacion; 0 = sin ajuste.
int TiltPercent() { return int(SendMessageW(g_tilt, TBM_GETPOS, 0, 0)); }
int TiltThreshold() { return TiltPercent() * 32767 / 100; }

void UpdateTiltLabel() {
  wchar_t text[96];
  int percent = TiltPercent();
  if (percent == 0) {
    swprintf(text, 96, T(kTiltOff));
  } else {
    swprintf(text, 96, T(kTiltValue), percent);
  }
  SetWindowTextW(g_tilt_label, text);
}

void ShowPage(int page) {
  for (int i = 0; i < kPageCount; ++i) {
    for (HWND control : g_pages[i]) {
      ShowWindow(control, i == page ? SW_SHOW : SW_HIDE);
    }
  }
}

// Cabecera STFS: tipo de contenido en 0x344, juego en 0x360 y nombre (UTF-16 big endian,
// primer idioma) en 0x411. Igual que en el SDK (content_manager.cpp).
bool ReadStfsHeader(const fs::path& path, DlcPackage& package) {
  uint8_t header[0x491];
  FILE* file = _wfopen(path.c_str(), L"rb");
  if (!file) {
    return false;
  }
  size_t read = fread(header, 1, sizeof(header), file);
  fclose(file);
  if (read != sizeof(header) ||
      (memcmp(header, "LIVE", 4) && memcmp(header, "PIRS", 4) && memcmp(header, "CON ", 4))) {
    return false;
  }
  auto load_be32 = [&header](size_t offset) {
    return (uint32_t(header[offset]) << 24) | (uint32_t(header[offset + 1]) << 16) |
           (uint32_t(header[offset + 2]) << 8) | uint32_t(header[offset + 3]);
  };
  package.content_type = load_be32(0x344);
  package.title_id = load_be32(0x360);
  for (size_t offset = 0x411; offset + 1 < sizeof(header); offset += 2) {
    wchar_t c = wchar_t((header[offset] << 8) | header[offset + 1]);
    if (!c) {
      break;
    }
    package.display_name.push_back(c);
  }
  return true;
}

// Los paquetes oficiales de Xbox Live se llaman con 42 caracteres hexadecimales.
bool HasOfficialFileName(const std::wstring& name) {
  if (name.size() != 42) {
    return false;
  }
  for (wchar_t c : name) {
    if (!iswxdigit(c)) {
      return false;
    }
  }
  return true;
}

bool IsCompatibleTitle(uint32_t title_id) {
  for (uint32_t id : kCompatibleTitles) {
    if (id == title_id) {
      return true;
    }
  }
  return false;
}

std::wstring GameName(const DlcPackage& package) {
  wchar_t buffer[96];
  if (package.content_type != 2) {
    swprintf(buffer, 96, T(kGameNotDlc), package.content_type);
    return buffer;
  }
  switch (package.title_id) {
    case 0x41560883:
      return T(kGameWoR);
    case 0x41560840:
      return T(kGameWorldTour);
    default:
      break;
  }
  swprintf(buffer, 96, IsCompatibleTitle(package.title_id) ? T(kGameOther) : T(kGameIncompatible),
           package.title_id);
  return buffer;
}

std::vector<DlcPackage> ScanDlc() {
  std::vector<DlcPackage> packages;
  fs::path folder = g_dir / kDlcFolder;
  std::error_code ec;
  if (!fs::is_directory(folder, ec)) {
    return packages;
  }
  for (auto it = fs::recursive_directory_iterator(
           folder, fs::directory_options::skip_permission_denied, ec);
       !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (!it->is_regular_file(ec) || it->file_size(ec) < 0x1000) {
      continue;
    }
    DlcPackage package;
    if (!ReadStfsHeader(it->path(), package)) {
      continue;
    }
    package.size = it->file_size(ec);
    package.relative_path = fs::relative(it->path(), folder, ec).wstring();
    package.official = HasOfficialFileName(it->path().filename().wstring());
    package.cache = package.display_name.find(L"Customs Database") != std::wstring::npos;
    if (package.display_name.empty()) {
      package.display_name = it->path().filename().wstring();
    }
    packages.push_back(std::move(package));
  }
  return packages;
}

void AddListRow(HWND list, const DlcPackage& package, const std::wstring& game) {
  LVITEMW item = {};
  item.mask = LVIF_TEXT;
  item.iItem = ListView_GetItemCount(list);
  item.pszText = const_cast<wchar_t*>(package.display_name.c_str());
  int row = ListView_InsertItem(list, &item);
  ListView_SetItemText(list, row, 1, const_cast<wchar_t*>(game.c_str()));
  wchar_t size[32];
  swprintf(size, 32, L"%.0f MB", double(package.size) / (1024.0 * 1024.0));
  ListView_SetItemText(list, row, 2, size);
  ListView_SetItemText(list, row, 3, const_cast<wchar_t*>(package.relative_path.c_str()));
}

void RefreshDlc() {
  ListView_DeleteAllItems(g_official_list);
  ListView_DeleteAllItems(g_custom_list);
  int custom_count = 0, cache_count = 0;
  for (const DlcPackage& package : ScanDlc()) {
    std::wstring game = GameName(package);
    if (package.cache) {
      game = T(kGameCache);
      ++cache_count;
    }
    if (package.official) {
      AddListRow(g_official_list, package, game);
    } else {
      AddListRow(g_custom_list, package, game);
      if (!package.cache) {
        ++custom_count;
      }
    }
  }
  Text status;
  if (custom_count == 0) {
    status = cache_count ? kStatusNoCustomCache : kStatusNoCustom;
  } else if (cache_count == 0) {
    status = kStatusMissingCache;
  } else if (cache_count > 1) {
    status = kStatusManyCaches;
  } else {
    status = kStatusCacheOk;
  }
  g_status_warning = status == kStatusMissingCache || status == kStatusManyCaches;
  SetWindowTextW(g_custom_status, T(status));
}

std::wstring GetName() {
  wchar_t buffer[64];
  GetWindowTextW(g_name, buffer, 64);
  std::wstring name = buffer;
  while (!name.empty() && name.back() == L' ') {
    name.pop_back();
  }
  while (!name.empty() && name.front() == L' ') {
    name.erase(name.begin());
  }
  return name;
}

bool IsValidName(const std::wstring& name) {
  if (name.empty() || name.size() > kMaxNameLength) {
    return false;
  }
  for (wchar_t c : name) {
    if (!(c < 128 && (iswalnum(c) || c == L' '))) {
      return false;
    }
  }
  return true;
}

void SaveSettings() {
  WriteIni(L"idioma", kLanguages[g_language].code);
  WriteIni(L"resolucion",
           SendMessageW(g_resolution, CB_GETCURSEL, 0, 0) == 1 ? L"x2" : L"720p");
  WriteIni(L"nombre", GetName());
  WriteIni(L"log", SendMessageW(g_log, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0");
  WriteIni(L"guitarra_nativa",
           SendMessageW(g_native_guitar, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0");
  WriteIni(L"tilt", std::to_wstring(TiltPercent()));
  WriteIni(L"teclado",
           SendMessageW(g_keyboard_enable, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0");
  WriteIni(L"teclas", KeysText());
  WriteIni(L"prioridad", PriorityText());
  WriteIni(L"anim60", SendMessageW(g_anim60, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0");
}

void Play() {
  std::wstring name = GetName();
  if (!IsValidName(name)) {
    MessageBoxW(g_window, T(kInvalidName), T(kInvalidNameTitle), MB_ICONWARNING);
    return;
  }
  fs::path game = g_dir / kGameExe;
  std::error_code ec;
  if (!fs::exists(game, ec)) {
    MessageBoxW(g_window, (std::wstring(T(kGameNotFound)) + game.wstring()).c_str(), T(kError),
                MB_ICONERROR);
    return;
  }
  if (!fs::exists(g_dir / L"assets" / L"default.xex", ec)) {
    MessageBoxW(g_window, T(kMissingAssets), T(kError), MB_ICONWARNING);
    return;
  }
  SaveSettings();

  std::wstring command = L"\"" + game.wstring() + L"\"";
  command += L" --user_language=" + std::to_wstring(kLanguages[g_language].xbox_id);
  if (SendMessageW(g_resolution, CB_GETCURSEL, 0, 0) == 1) {
    command +=
        L" --draw_resolution_scale_x=2 --draw_resolution_scale_y=2"
        L" --texture_cache_memory_limit_render_to_texture=256"
        L" --resolve_resolution_scale_fill_half_pixel_offset=true";
  }
  command += L" \"--user_name=" + name + L"\"";
  command += SendMessageW(g_native_guitar, BM_GETCHECK, 0, 0) == BST_CHECKED
                 ? L" --native_rb4_guitar=true"
                 : L" --native_rb4_guitar=false";
  command += L" --tilt_threshold=" + std::to_wstring(TiltThreshold());
  command += SendMessageW(g_keyboard_enable, BM_GETCHECK, 0, 0) == BST_CHECKED
                 ? L" --keyboard_guitar=true"
                 : L" --keyboard_guitar=false";
  command += L" --keyboard_map=" + KeysText();
  command += L" --input_priority=" + PriorityText();
  // El juego usa a veces texturas con el tipo de "ficha" invalido; sin esto el SDK no las
  // dibuja en ese cuadro y el selector de personajes parpadea.
  command += L" --gpu_allow_invalid_fetch_constants=true";
  if (SendMessageW(g_anim60, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    // Logica del juego a 60: vblank al doble (el juego avanza un cuadro cada 2 vblanks). El
    // reloj de la cancion usa tiempo real (fix_song_time), asi que las notas no se adelantan.
    command += L" --vblank_hz=120 --cpu_patch_render=false";
  }
  if (SendMessageW(g_log, BM_GETCHECK, 0, 0) == BST_CHECKED) {
    // El log anterior se conserva para poder revisar un crash despues de reabrir el juego.
    fs::path logs = g_dir / L"logs";
    fs::create_directories(logs, ec);
    fs::path log = logs / L"juego.log";
    if (fs::exists(log, ec)) {
      fs::rename(log, logs / L"juego_anterior.log", ec);
    }
    command += L" \"--log_file=" + log.wstring() + L"\"";
  }

  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(game.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      g_dir.c_str(), &startup, &process)) {
    MessageBoxW(g_window, T(kCannotStart), T(kError), MB_ICONERROR);
    return;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  DestroyWindow(g_window);
}

HWND MakeControl(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h,
                 int id, HFONT font = nullptr) {
  HWND control = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                                 g_window, reinterpret_cast<HMENU>(intptr_t(id)),
                                 GetModuleHandleW(nullptr), nullptr);
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : g_font), TRUE);
  if (g_building_page >= 0) {
    g_pages[g_building_page].push_back(control);
  }
  return control;
}

HWND MakeList(int y, int height, int id) {
  HWND list = MakeControl(WC_LISTVIEWW, L"", WS_BORDER | LVS_REPORT | LVS_SINGLESEL, 24, y, 632,
                          height, id);
  ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  ListView_SetBkColor(list, kListBack);
  ListView_SetTextBkColor(list, kListBack);
  ListView_SetTextColor(list, kTextColor);
  const Text titles[] = {kColName, kColGame, kColSize, kColFile};
  const int widths[] = {250, 160, 70, 130};
  for (int i = 0; i < 4; ++i) {
    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<wchar_t*>(T(titles[i]));
    column.cx = widths[i];
    ListView_InsertColumn(list, i, &column);
  }
  return list;
}

// Crea todos los controles con los textos del idioma actual y los valores de launcher.ini.
void CreateControls(int selected_page) {
  g_building_page = -1;
  g_title = MakeControl(L"STATIC", L"Guitar Hero: Warriors of Rock  -  re-wor", 0, 20, 12, 640, 24, 0,
              g_bold_font);
  g_tabs = MakeControl(WC_TABCONTROLW, L"", WS_CLIPSIBLINGS | WS_TABSTOP, 10, 44, 660, 30,
                       kIdTabs);
  const Text tab_titles[kPageCount] = {kTabSettings, kTabOfficial, kTabCustom, kTabControls,
                                       kTabKeyboard};
  for (int i = 0; i < kPageCount; ++i) {
    TCITEMW tab = {};
    tab.mask = TCIF_TEXT;
    tab.pszText = const_cast<wchar_t*>(T(tab_titles[i]));
    TabCtrl_InsertItem(g_tabs, i, &tab);
  }
  // La barra de pestanas solo ocupa lo que miden las pestanas: el resto deja ver el fondo.
  RECT last_tab;
  TabCtrl_GetItemRect(g_tabs, kPageCount - 1, &last_tab);
  SetWindowPos(g_tabs, nullptr, 0, 0, last_tab.right + 4, last_tab.bottom + 2,
               SWP_NOMOVE | SWP_NOZORDER);

  // Ajustes.
  g_building_page = kPageSettings;
  MakeControl(L"STATIC", T(kLabelLanguage), 0, 30, 100, 140, 20, 0);
  g_language_box = MakeControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 180, 96, 260, 200,
                               kIdLanguage);
  for (const Language& language : kLanguages) {
    SendMessageW(g_language_box, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(language.name));
  }
  SendMessageW(g_language_box, CB_SETCURSEL, g_language, 0);

  MakeControl(L"STATIC", T(kLabelResolution), 0, 30, 142, 140, 20, 0);
  g_resolution = MakeControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 180, 138, 260, 200,
                             kIdResolution);
  SendMessageW(g_resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(kRes720)));
  SendMessageW(g_resolution, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(T(kResX2)));
  SendMessageW(g_resolution, CB_SETCURSEL, ReadIni(L"resolucion", L"720p") == L"x2" ? 1 : 0, 0);

  MakeControl(L"STATIC", T(kLabelName), 0, 30, 187, 150, 20, 0);
  g_name = MakeControl(L"EDIT", ReadIni(L"nombre", L"User").c_str(),
                       WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 180, 184, 260, 24, kIdName);
  SendMessageW(g_name, EM_SETLIMITTEXT, kMaxNameLength, 0);
  MakeControl(L"STATIC", T(kNameHint), 0, 180, 212, 470, 20, 0);
  g_anim60 = MakeControl(L"BUTTON", T(kAnim60), BS_AUTOCHECKBOX | WS_TABSTOP, 30, 248, 560, 22,
                         kIdAnim60);
  SetWindowTheme(g_anim60, L"", L"");
  SendMessageW(g_anim60, BM_SETCHECK, ReadIni(L"anim60", L"0") == L"1" ? BST_CHECKED : BST_UNCHECKED,
               0);
  MakeControl(L"STATIC", T(kAnim60Hint), 0, 50, 272, 600, 20, 0);
  g_log = MakeControl(L"BUTTON", T(kSaveLog), BS_AUTOCHECKBOX | WS_TABSTOP, 30, 304, 450, 22,
                      kIdLog);
  SetWindowTheme(g_log, L"", L"");
  SendMessageW(g_log, BM_SETCHECK, ReadIni(L"log", L"1") == L"1" ? BST_CHECKED : BST_UNCHECKED,
               0);
  MakeControl(L"STATIC", T(kSettingsNote), 0, 30, 345, 620, 70, 0);

  // DLC compatibles.
  g_building_page = kPageOfficial;
  MakeControl(L"STATIC", T(kOfficialHeader), 0, 24, 84, 632, 20, 0);
  g_official_list = MakeList(108, 352, kIdOfficialList);

  // DLC custom.
  g_building_page = kPageCustom;
  MakeControl(L"STATIC", T(kCustomHeader), 0, 24, 84, 632, 20, 0);
  g_custom_list = MakeList(108, 280, kIdCustomList);
  g_custom_status = MakeControl(L"STATIC", L"", 0, 24, 396, 632, 24, kIdCustomStatus);
  MakeControl(L"BUTTON", T(kCacheHelpButton), WS_TABSTOP, 24, 428, 260, 30, kIdCacheHelp);

  // Controles.
  g_building_page = kPageControls;
  g_native_guitar = MakeControl(L"BUTTON", T(kNativeGuitar), BS_AUTOCHECKBOX | WS_TABSTOP, 30, 96,
                                560, 22, kIdNativeGuitar);
  SetWindowTheme(g_native_guitar, L"", L"");
  SendMessageW(g_native_guitar, BM_SETCHECK,
               ReadIni(L"guitarra_nativa", L"1") == L"1" ? BST_CHECKED : BST_UNCHECKED, 0);
  MakeControl(L"STATIC", T(kNativeGuitarHint), 0, 50, 122, 610, 40, 0);
  MakeControl(L"STATIC", T(kTiltLabel), 0, 30, 185, 500, 20, 0);
  g_tilt = MakeControl(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, 30, 210, 420,
                       32, kIdTilt);
  SendMessageW(g_tilt, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
  SendMessageW(g_tilt, TBM_SETTICFREQ, 10, 0);
  SendMessageW(g_tilt, TBM_SETPOS, TRUE, _wtoi(ReadIni(L"tilt", L"49").c_str()));
  g_tilt_label = MakeControl(L"STATIC", L"", 0, 460, 216, 210, 20, kIdTiltLabel);
  UpdateTiltLabel();
  MakeControl(L"STATIC", T(kTiltHint), 0, 30, 248, 630, 20, 0);
  MakeControl(L"STATIC", T(kPriorityLabel), 0, 30, 282, 630, 20, 0);
  g_priority_list = MakeControl(L"LISTBOX", L"", WS_BORDER | LBS_NOTIFY | WS_TABSTOP, 30, 306,
                                420, 74, kIdPriorityList);
  MakeControl(L"BUTTON", T(kPriorityUp), WS_TABSTOP, 462, 306, 120, 32, kIdPriorityUp);
  MakeControl(L"BUTTON", T(kPriorityDown), WS_TABSTOP, 462, 344, 120, 32, kIdPriorityDown);
  UpdatePriorityList(0);
  MakeControl(L"STATIC", T(kKeysHelp), 0, 30, 392, 620, 70, 0);

  // Mapeado de teclado.
  g_building_page = kPageKeyboard;
  g_keyboard_enable = MakeControl(L"BUTTON", T(kKeyboardEnable), BS_AUTOCHECKBOX | WS_TABSTOP, 30,
                                  96, 560, 22, kIdKeyboardEnable);
  SetWindowTheme(g_keyboard_enable, L"", L"");
  SendMessageW(g_keyboard_enable, BM_SETCHECK,
               ReadIni(L"teclado", L"1") == L"1" ? BST_CHECKED : BST_UNCHECKED, 0);
  MakeControl(L"STATIC", T(kKeyboardHint), 0, 30, 124, 630, 20, 0);
  for (int i = 0; i < kKeyCount; ++i) {
    int column = i / 6, row = i % 6;
    int x = 30 + column * 320, y = 160 + row * 42;
    MakeControl(L"STATIC", T(Text(kKeyGreen + i)), 0, x, y + 6, 150, 20, 0);
    g_key_buttons[i] = MakeControl(L"BUTTON", L"", WS_TABSTOP, x + 155, y, 140, 32, kIdKeyBase + i);
  }
  MakeControl(L"BUTTON", T(kRestoreKeys), WS_TABSTOP, 30, 420, 240, 30, kIdRestoreKeys);
  UpdateKeyButtons();

  // Botones (siempre visibles).
  g_building_page = -1;
  MakeControl(L"BUTTON", T(kOpenFolder), WS_TABSTOP, 10, 490, 190, 32, kIdOpenFolder);
  MakeControl(L"BUTTON", T(kRefresh), WS_TABSTOP, 210, 490, 160, 32, kIdRefresh);
  MakeControl(L"BUTTON", T(kExit), WS_TABSTOP, 450, 490, 100, 32, kIdExit);
  MakeControl(L"BUTTON", T(kPlay), BS_DEFPUSHBUTTON | WS_TABSTOP, 560, 490, 110, 32, kIdPlay,
              g_bold_font);
  RefreshDlc();
  TabCtrl_SetCurSel(g_tabs, selected_page);
  ShowPage(selected_page);
}

// Cambia el idioma: guarda lo elegido y vuelve a crear la ventana con los textos nuevos.
void ChangeLanguage(int language) {
  if (language < 0 || language >= kLanguageCount || language == g_language) {
    return;
  }
  int page = TabCtrl_GetCurSel(g_tabs);
  g_language = language;
  SaveSettings();
  SendMessageW(g_window, WM_SETREDRAW, FALSE, 0);
  while (HWND child = GetWindow(g_window, GW_CHILD)) {
    DestroyWindow(child);
  }
  for (auto& controls : g_pages) {
    controls.clear();
  }
  CreateControls(page);
  SendMessageW(g_window, WM_SETREDRAW, TRUE, 0);
  RedrawWindow(g_window, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
  SetFocus(g_language_box);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE: {
      g_window = window;
      RECT client;
      GetClientRect(window, &client);
      CreateBackground(client.right, client.bottom);
      CreateControls(kPageSettings);
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wparam) >= kIdKeyBase && LOWORD(wparam) < kIdKeyBase + kKeyCount) {
        g_capture = LOWORD(wparam) - kIdKeyBase;  // la proxima tecla se asigna ahi
        UpdateKeyButtons();
        return 0;
      }
      switch (LOWORD(wparam)) {
        case kIdLanguage:
          if (HIWORD(wparam) == CBN_SELCHANGE) {
            ChangeLanguage(int(SendMessageW(g_language_box, CB_GETCURSEL, 0, 0)));
          }
          return 0;
        case kIdPlay:
          Play();
          return 0;
        case kIdOpenFolder: {
          fs::path folder = g_dir / kDlcFolder;
          std::error_code ec;
          fs::create_directories(folder, ec);
          ShellExecuteW(window, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
          return 0;
        }
        case kIdRefresh:
          RefreshDlc();
          return 0;
        case kIdCacheHelp:
          MessageBoxW(window, T(kCacheHelp), T(kCacheHelpTitle), MB_ICONINFORMATION);
          return 0;
        case kIdPriorityUp:
          MovePriority(-1);
          return 0;
        case kIdPriorityDown:
          MovePriority(1);
          return 0;
        case kIdRestoreKeys:
          for (int i = 0; i < kKeyCount; ++i) {
            g_keys[i] = kDefaultKeys[i];
          }
          g_capture = -1;
          UpdateKeyButtons();
          return 0;
        case kIdExit:
          SaveSettings();
          DestroyWindow(window);
          return 0;
      }
      break;
    case WM_NOTIFY: {
      auto* header = reinterpret_cast<NMHDR*>(lparam);
      if (header->hwndFrom == g_tabs && header->code == TCN_SELCHANGE) {
        ShowPage(TabCtrl_GetCurSel(g_tabs));
      }
      break;
    }
    case WM_HSCROLL:
      if (reinterpret_cast<HWND>(lparam) == g_tilt) {
        UpdateTiltLabel();
      }
      return 0;
    case WM_ERASEBKGND:
      PaintBackground(reinterpret_cast<HDC>(wparam));
      return 1;
    case WM_PRINTCLIENT:
      PaintBackground(reinterpret_cast<HDC>(wparam));
      return 0;
    case WM_CTLCOLORLISTBOX: {
      static HBRUSH list_brush = CreateSolidBrush(kListBack);
      HDC dc = reinterpret_cast<HDC>(wparam);
      SetTextColor(dc, kTextColor);
      SetBkColor(dc, kListBack);
      return reinterpret_cast<LRESULT>(list_brush);
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      HWND control = reinterpret_cast<HWND>(lparam);
      SetBkMode(dc, TRANSPARENT);
      if (control == g_custom_status) {
        SetTextColor(dc, g_status_warning ? RGB(255, 110, 90) : RGB(140, 225, 140));
      } else if (control == g_title) {
        SetTextColor(dc, kTitleColor);
      } else {
        SetTextColor(dc, kTextColor);
      }
      return reinterpret_cast<LRESULT>(BackgroundBrushFor(control, dc));
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  SetProcessDPIAware();
  wchar_t exe_path[MAX_PATH];
  GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
  g_dir = fs::path(exe_path).parent_path();
  g_language = LanguageFromIni();
  LoadKeys();
  LoadPriority();
  Gdiplus::GdiplusStartupInput gdiplus_input;
  ULONG_PTR gdiplus_token;
  Gdiplus::GdiplusStartup(&gdiplus_token, &gdiplus_input, nullptr);

  INITCOMMONCONTROLSEX controls = {
      sizeof(controls),
      ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES};
  InitCommonControlsEx(&controls);
  g_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                       0, L"Segoe UI");
  g_bold_font = CreateFontW(-17, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Segoe UI");

  WNDCLASSEXW window_class = {sizeof(window_class)};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;  // lo pinta WM_ERASEBKGND (imagen de fondo)
  window_class.hIcon = LoadIconW(instance, L"IDI_ICON1");
  window_class.lpszClassName = L"ReWorLauncher";
  RegisterClassExW(&window_class);

  RECT rect = {0, 0, 680, 534};
  DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRect(&rect, style, FALSE);
  HWND window = CreateWindowExW(0, window_class.lpszClassName, L"re-wor - Launcher", style,
                                CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                                rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
  ShowWindow(window, show);

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    // Cambio de tecla en "Mapeado de teclado": la tecla apretada se asigna (Esc cancela).
    if (g_capture >= 0 && (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN)) {
      int vk = int(message.wParam);
      if (vk != VK_ESCAPE) {
        g_keys[g_capture] = vk;
      }
      g_capture = -1;
      UpdateKeyButtons();
      continue;
    }
    if (g_capture >= 0 && (message.message == WM_KEYUP || message.message == WM_SYSKEYUP ||
                           message.message == WM_CHAR || message.message == WM_SYSCHAR)) {
      continue;
    }
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  return 0;
}
