// Author: Ivan Stojmenovic
#include "framework.h"
#include "Loc.h"

namespace
{
    UiLang g_lang = UiLang::En;
    constexpr int kLangCount = static_cast<int>(UiLang::Count);

    const wchar_t* kLangNames[kLangCount] = {
        L"English", L"Srpski", L"Deutsch", L"Français",
        L"Español", L"Italiano", L"Русский", L"Hrvatski"
    };

    struct Entry { const wchar_t* t[kLangCount]; };

    const Entry kStrings[static_cast<size_t>(StrId::Count)] = {
        { L"Capture region", L"Snimi region", L"Bereich aufnehmen", L"Capturer la zone", L"Capturar región", L"Cattura area", L"Снять область", L"Snimi područje" },
        { L"Options...", L"Opcije...", L"Optionen...", L"Options...", L"Opciones...", L"Opzioni...", L"Параметры...", L"Opcije..." },
        { L"About...", L"O programu...", L"Info...", L"À propos...", L"Acerca de...", L"Informazioni...", L"О программе...", L"O programu..." },
        { L"Share", L"Podeli", L"Teilen", L"Partager", L"Compartir", L"Condividi", L"Поделиться", L"Podijeli" },
        { L"Copy last link", L"Kopiraj poslednji link", L"Letzten Link kopieren", L"Copier le dernier lien", L"Copiar último enlace", L"Copia ultimo link", L"Копировать последнюю ссылку", L"Kopiraj zadnji link" },
        { L"Open last link", L"Otvori poslednji link", L"Letzten Link öffnen", L"Ouvrir le dernier lien", L"Abrir último enlace", L"Apri ultimo link", L"Открыть последнюю ссылку", L"Otvori zadnji link" },
        { L"Exit", L"Izlaz", L"Beenden", L"Quitter", L"Salir", L"Esci", L"Выход", L"Izlaz" },
        { L"Screenshot copied to clipboard", L"Snimak kopiran u clipboard", L"Screenshot in Zwischenablage kopiert", L"Capture copiée dans le presse-papiers", L"Captura copiada al portapapeles", L"Screenshot copiato negli appunti", L"Снимок скопирован в буфер", L"Snimka kopirana u međuspremnik" },
        { L"Screenshot saved", L"Snimak sačuvan", L"Screenshot gespeichert", L"Capture enregistrée", L"Captura guardada", L"Screenshot salvato", L"Снимок сохранён", L"Snimka spremljena" },
        { L"Uploaded", L"Otpremljeno", L"Hochgeladen", L"Téléversé", L"Subido", L"Caricato", L"Загружено", L"Preneseno" },
        { L"Upload failed", L"Otpremanje nije uspelo", L"Upload fehlgeschlagen", L"Échec du téléversement", L"Error al subir", L"Caricamento non riuscito", L"Ошибка загрузки", L"Prijenos nije uspio" },
        { L"Upload is disabled in Options", L"Upload je isključen u Opcijama", L"Upload ist in den Optionen deaktiviert", L"Le téléversement est désactivé dans Options", L"La subida está desactivada en Opciones", L"Il caricamento è disabilitato in Opzioni", L"Загрузка отключена в параметрах", L"Prijenos je isključen u Opcijama" },
        { L"Link copied to clipboard", L"Link kopiran u clipboard", L"Link in Zwischenablage kopiert", L"Lien copié dans le presse-papiers", L"Enlace copiado al portapapeles", L"Link copiato negli appunti", L"Ссылка скопирована в буфер", L"Link kopiran u međuspremnik" },
        { L"Sent to printer", L"Poslato na štampač", L"An Drucker gesendet", L"Envoyé à l'imprimante", L"Enviado a la impresora", L"Inviato alla stampante", L"Отправлено на принтер", L"Poslano na pisač" },
        { L"Opened image search in browser", L"Pretraga slike otvorena u pregledaču", L"Bildersuche im Browser geöffnet", L"Recherche d'image ouverte dans le navigateur", L"Búsqueda de imagen abierta en el navegador", L"Ricerca immagine aperta nel browser", L"Поиск по картинке открыт в браузере", L"Pretraživanje slike otvoreno u pregledniku" },
        { L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot" },
        { L"Options", L"Opcije", L"Optionen", L"Options", L"Opciones", L"Opzioni", L"Параметры", L"Opcije" },
        { L"General", L"Opšte", L"Allgemein", L"Général", L"General", L"Generale", L"Общие", L"Općenito" },
        { L"Hotkeys", L"Prečice", L"Tastenkürzel", L"Raccourcis", L"Atajos", L"Scorciatoie", L"Горячие клавиши", L"Prečaci" },
        { L"Formats", L"Formati", L"Formate", L"Formats", L"Formatos", L"Formati", L"Форматы", L"Formati" },
        { L"Cloud", L"Cloud", L"Cloud", L"Cloud", L"Nube", L"Cloud", L"Облако", L"Cloud" },
        { L"Proxy", L"Proxy", L"Proxy", L"Proxy", L"Proxy", L"Proxy", L"Прокси", L"Proxy" },
        { L"Show notifications about copying and saving", L"Prikaži obaveštenja o kopiranju i čuvanju", L"Benachrichtigungen zu Kopieren und Speichern anzeigen", L"Afficher les notifications de copie et d'enregistrement", L"Mostrar notificaciones de copiar y guardar", L"Mostra notifiche di copia e salvataggio", L"Показывать уведомления о копировании и сохранении", L"Prikaži obavijesti o kopiranju i spremanju" },
        { L"Keep the selected area position", L"Zadrži poziciju selektovane oblasti", L"Position des ausgewählten Bereichs behalten", L"Conserver la position de la zone sélectionnée", L"Mantener la posición del área seleccionada", L"Mantieni la posizione dell'area selezionata", L"Сохранять позицию выбранной области", L"Zadrži položaj odabranog područja" },
        { L"Capture a cursor on a screenshot", L"Uključi kursor na snimku", L"Mauszeiger im Screenshot erfassen", L"Inclure le curseur sur la capture", L"Incluir el cursor en la captura", L"Includi il cursore nello screenshot", L"Захватывать курсор на снимке", L"Uključi pokazivač na snimci" },
        { L"Autostart (Start with Windows)", L"Autostart (pokreni sa Windowsom)", L"Autostart (mit Windows starten)", L"Démarrage automatique (avec Windows)", L"Inicio automático (con Windows)", L"Avvio automatico (con Windows)", L"Автозапуск (с Windows)", L"Autostart (pokreni s Windowsom)" },
        { L"General hotkey", L"Glavna prečica", L"Allgemeines Tastenkürzel", L"Raccourci général", L"Atajo general", L"Scorciatoia generale", L"Основная горячая клавиша", L"Glavni prečac" },
        { L"Instant save of the fullscreen", L"Trenutno čuvanje celog ekrana", L"Sofortiges Speichern des Vollbilds", L"Enregistrement instantané plein écran", L"Guardado instantáneo de pantalla completa", L"Salvataggio istantaneo a schermo intero", L"Мгновенное сохранение всего экрана", L"Trenutno spremanje cijelog zaslona" },
        { L"Click a hotkey field, then press the desired key combination.", L"Klikni polje prečice, pa pritisni željenu kombinaciju tastera.", L"Klicken Sie auf ein Tastenkürzelfeld und drücken Sie die gewünschte Kombination.", L"Cliquez sur un champ de raccourci, puis appuyez sur la combinaison.", L"Haz clic en un campo de atajo y pulsa la combinación deseada.", L"Fai clic su un campo scorciatoia, poi premi la combinazione.", L"Нажмите поле горячей клавиши, затем нужную комбинацию.", L"Klikni polje prečaca, zatim pritisni željenu kombinaciju." },
        { L"Press a key...", L"Pritisni taster...", L"Taste drücken...", L"Appuyez sur une touche...", L"Pulsa una tecla...", L"Premi un tasto...", L"Нажмите клавишу...", L"Pritisni tipku..." },
        { L"Language:", L"Jezik:", L"Sprache:", L"Langue :", L"Idioma:", L"Lingua:", L"Язык:", L"Jezik:" },
        { L"Default format:", L"Podrazumevani format:", L"Standardformat:", L"Format par défaut :", L"Formato predeterminado:", L"Formato predefinito:", L"Формат по умолчанию:", L"Zadani format:" },
        { L"JPEG quality (1-100):", L"JPEG kvalitet (1-100):", L"JPEG-Qualität (1-100):", L"Qualité JPEG (1-100) :", L"Calidad JPEG (1-100):", L"Qualità JPEG (1-100):", L"Качество JPEG (1-100):", L"JPEG kvaliteta (1-100):" },
        { L"Filename pattern:", L"Šablon imena fajla:", L"Dateinamensmuster:", L"Modèle de nom de fichier :", L"Patrón de nombre:", L"Schema nome file:", L"Шаблон имени файла:", L"Predložak naziva datoteke:" },
        { L"Placeholders: {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Placeholders: {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Platzhalter: {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Espaces réservés : {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Marcadores: {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Segnaposto: {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Подстановки: {yyyy} {MM} {dd} {HH} {mm} {ss}", L"Zamjene: {yyyy} {MM} {dd} {HH} {mm} {ss}" },
        { L"Default save folder:", L"Podrazumevani folder za čuvanje:", L"Standard-Speicherordner:", L"Dossier d'enregistrement par défaut :", L"Carpeta de guardado predeterminada:", L"Cartella di salvataggio predefinita:", L"Папка сохранения по умолчанию:", L"Zadana mapa za spremanje:" },
        { L"Browse...", L"Izaberi...", L"Durchsuchen...", L"Parcourir...", L"Examinar...", L"Sfoglia...", L"Обзор...", L"Pregledaj..." },
        { L"Choose default save folder", L"Izaberi folder za čuvanje", L"Standard-Speicherordner wählen", L"Choisir le dossier d'enregistrement", L"Elegir carpeta de guardado", L"Scegli cartella di salvataggio", L"Выберите папку сохранения", L"Odaberi mapu za spremanje" },
        { L"English", L"English", L"English", L"English", L"English", L"English", L"English", L"English" },
        { L"Srpski", L"Srpski", L"Srpski", L"Srpski", L"Srpski", L"Srpski", L"Srpski", L"Srpski" },
        { L"Enable image upload", L"Omogući upload slike", L"Bild-Upload aktivieren", L"Activer le téléversement d'images", L"Habilitar subida de imágenes", L"Abilita caricamento immagini", L"Включить загрузку изображений", L"Omogući prijenos slike" },
        { L"Upload URL:", L"Upload URL:", L"Upload-URL:", L"URL de téléversement :", L"URL de subida:", L"URL di caricamento:", L"URL загрузки:", L"URL prijenosa:" },
        { L"Server should accept multipart field \"file\" and return a URL (text or JSON).", L"Server treba da primi multipart polje \"file\" i vrati URL (tekst ili JSON).", L"Der Server sollte das Multipart-Feld \"file\" annehmen und eine URL zurückgeben.", L"Le serveur doit accepter le champ multipart \"file\" et renvoyer une URL.", L"El servidor debe aceptar el campo multipart \"file\" y devolver una URL.", L"Il server deve accettare il campo multipart \"file\" e restituire un URL.", L"Сервер должен принять multipart-поле \"file\" и вернуть URL.", L"Poslužitelj treba primiti multipart polje \"file\" i vratiti URL." },
        { L"Use HTTP proxy for upload", L"Koristi HTTP proxy za upload", L"HTTP-Proxy für Upload verwenden", L"Utiliser un proxy HTTP pour le téléversement", L"Usar proxy HTTP para subir", L"Usa proxy HTTP per il caricamento", L"Использовать HTTP-прокси для загрузки", L"Koristi HTTP proxy za prijenos" },
        { L"Host:", L"Host:", L"Host:", L"Hôte :", L"Host:", L"Host:", L"Хост:", L"Host:" },
        { L"Port:", L"Port:", L"Port:", L"Port :", L"Puerto:", L"Porta:", L"Порт:", L"Port:" },
        { L"Username:", L"Korisničko ime:", L"Benutzername:", L"Nom d'utilisateur :", L"Usuario:", L"Nome utente:", L"Имя пользователя:", L"Korisničko ime:" },
        { L"Password:", L"Lozinka:", L"Passwort:", L"Mot de passe :", L"Contraseña:", L"Password:", L"Пароль:", L"Lozinka:" },
        { L"When disabled, Windows system proxy is used. Username/password are optional.", L"Kad je isključeno, koristi se Windows sistemski proxy. Korisničko ime/lozinka su opcioni.", L"Wenn deaktiviert, wird der Windows-Systemproxy verwendet. Benutzer/Passwort optional.", L"Si désactivé, le proxy système Windows est utilisé. Identifiants optionnels.", L"Si está desactivado, se usa el proxy del sistema. Usuario/contraseña opcionales.", L"Se disabilitato, si usa il proxy di sistema Windows. Credenziali opzionali.", L"Если выключено — системный прокси Windows. Логин/пароль необязательны.", L"Kad je isključeno, koristi se Windows proxy. Korisničko ime/lozinka su opcionalni." },
        { L"Pencil", L"Olovka", L"Stift", L"Crayon", L"Lápiz", L"Matita", L"Карандаш", L"Olovka" },
        { L"Highlighter", L"Marker", L"Textmarker", L"Surligneur", L"Resaltador", L"Evidenziatore", L"Маркер", L"Marker" },
        { L"Line", L"Linija", L"Linie", L"Ligne", L"Línea", L"Linea", L"Линия", L"Linija" },
        { L"Arrow", L"Strelica", L"Pfeil", L"Flèche", L"Flecha", L"Freccia", L"Стрелка", L"Strelica" },
        { L"Rectangle", L"Pravougaonik", L"Rechteck", L"Rectangle", L"Rectángulo", L"Rettangolo", L"Прямоугольник", L"Pravokutnik" },
        { L"Ellipse", L"Elipsa", L"Ellipse", L"Ellipse", L"Elipse", L"Ellisse", L"Эллипс", L"Elipsa" },
        { L"Text", L"Tekst", L"Text", L"Texte", L"Texto", L"Testo", L"Текст", L"Tekst" },
        { L"Stroke color", L"Boja poteza", L"Strichfarbe", L"Couleur du trait", L"Color del trazo", L"Colore tratto", L"Цвет линии", L"Boja poteza" },
        { L"Undo (Ctrl+Click = Redo)", L"Undo (Ctrl+Click = Redo)", L"Rückgängig (Strg+Klick = Wiederholen)", L"Annuler (Ctrl+clic = Rétablir)", L"Deshacer (Ctrl+clic = Rehacer)", L"Annulla (Ctrl+clic = Ripeti)", L"Отмена (Ctrl+клик = Повтор)", L"Poništi (Ctrl+klik = Ponovi)" },
        { L"Thinner stroke", L"Tanji potez", L"Dünnerer Strich", L"Trait plus fin", L"Trazo más fino", L"Tratto più sottile", L"Тоньше линия", L"Tanji potez" },
        { L"Thicker stroke", L"Deblji potez", L"Dickerer Strich", L"Trait plus épais", L"Trazo más grueso", L"Tratto più spesso", L"Толще линия", L"Deblji potez" },
        { L"Copy to clipboard (Ctrl+C)", L"Kopiraj u clipboard (Ctrl+C)", L"In Zwischenablage kopieren (Strg+C)", L"Copier dans le presse-papiers (Ctrl+C)", L"Copiar al portapapeles (Ctrl+C)", L"Copia negli appunti (Ctrl+C)", L"Копировать в буфер (Ctrl+C)", L"Kopiraj u međuspremnik (Ctrl+C)" },
        { L"Save file (Ctrl+S)", L"Sačuvaj fajl (Ctrl+S)", L"Datei speichern (Strg+S)", L"Enregistrer le fichier (Ctrl+S)", L"Guardar archivo (Ctrl+S)", L"Salva file (Ctrl+S)", L"Сохранить файл (Ctrl+S)", L"Spremi datoteku (Ctrl+S)" },
        { L"Upload and get link", L"Otpremi i dobij link", L"Hochladen und Link erhalten", L"Téléverser et obtenir un lien", L"Subir y obtener enlace", L"Carica e ottieni link", L"Загрузить и получить ссылку", L"Prenesi i dobij link" },
        { L"Share", L"Podeli", L"Teilen", L"Partager", L"Compartir", L"Condividi", L"Поделиться", L"Podijeli" },
        { L"Cancel (Esc)", L"Otkaži (Esc)", L"Abbrechen (Esc)", L"Annuler (Échap)", L"Cancelar (Esc)", L"Annulla (Esc)", L"Отмена (Esc)", L"Odustani (Esc)" },
        { L"Bold", L"Podebljano", L"Fett", L"Gras", L"Negrita", L"Grassetto", L"Жирный", L"Podebljano" },
        { L"Italic", L"Kurziv", L"Kursiv", L"Italique", L"Cursiva", L"Corsivo", L"Курсив", L"Kurziv" },
        { L"Underline", L"Podvučeno", L"Unterstrichen", L"Souligné", L"Subrayado", L"Sottolineato", L"Подчёркнутый", L"Podcrtano" },
        { L"Larger text", L"Veći tekst", L"Größerer Text", L"Texte plus grand", L"Texto más grande", L"Testo più grande", L"Крупнее текст", L"Veći tekst" },
        { L"Smaller text", L"Manji tekst", L"Kleinerer Text", L"Texte plus petit", L"Texto más pequeño", L"Testo più piccolo", L"Мельче текст", L"Manji tekst" },
        { L"Text color", L"Boja teksta", L"Textfarbe", L"Couleur du texte", L"Color del texto", L"Colore testo", L"Цвет текста", L"Boja teksta" },
        { L"Copy to clipboard\tCtrl+C", L"Kopiraj u clipboard\tCtrl+C", L"In Zwischenablage kopieren\tStrg+C", L"Copier dans le presse-papiers\tCtrl+C", L"Copiar al portapapeles\tCtrl+C", L"Copia negli appunti\tCtrl+C", L"Копировать в буфер\tCtrl+C", L"Kopiraj u međuspremnik\tCtrl+C" },
        { L"Save file\tCtrl+S", L"Sačuvaj fajl\tCtrl+S", L"Datei speichern\tStrg+S", L"Enregistrer le fichier\tCtrl+S", L"Guardar archivo\tCtrl+S", L"Salva file\tCtrl+S", L"Сохранить файл\tCtrl+S", L"Spremi datoteku\tCtrl+S" },
        { L"Share", L"Podeli", L"Teilen", L"Partager", L"Compartir", L"Condividi", L"Поделиться", L"Podijeli" },
        { L"Upload\tCtrl+U", L"Upload\tCtrl+U", L"Hochladen\tStrg+U", L"Téléverser\tCtrl+U", L"Subir\tCtrl+U", L"Carica\tCtrl+U", L"Загрузить\tCtrl+U", L"Prijenos\tCtrl+U" },
        { L"Copy link", L"Kopiraj link", L"Link kopieren", L"Copier le lien", L"Copiar enlace", L"Copia link", L"Копировать ссылку", L"Kopiraj link" },
        { L"Open link in browser", L"Otvori link u pregledaču", L"Link im Browser öffnen", L"Ouvrir le lien dans le navigateur", L"Abrir enlace en el navegador", L"Apri link nel browser", L"Открыть ссылку в браузере", L"Otvori link u pregledniku" },
        { L"Print...\tCtrl+P", L"Štampaj...\tCtrl+P", L"Drucken...\tStrg+P", L"Imprimer...\tCtrl+P", L"Imprimir...\tCtrl+P", L"Stampa...\tCtrl+P", L"Печать...\tCtrl+P", L"Ispiši...\tCtrl+P" },
        { L"Search by image", L"Pretraži po slici", L"Nach Bild suchen", L"Rechercher par image", L"Buscar por imagen", L"Cerca per immagine", L"Поиск по картинке", L"Pretraži po slici" },
        { L"Undo\tCtrl+Z", L"Undo\tCtrl+Z", L"Rückgängig\tStrg+Z", L"Annuler\tCtrl+Z", L"Deshacer\tCtrl+Z", L"Annulla\tCtrl+Z", L"Отмена\tCtrl+Z", L"Poništi\tCtrl+Z" },
        { L"Redo\tCtrl+Y", L"Redo\tCtrl+Y", L"Wiederholen\tStrg+Y", L"Rétablir\tCtrl+Y", L"Rehacer\tCtrl+Y", L"Ripeti\tCtrl+Y", L"Повтор\tCtrl+Y", L"Ponovi\tCtrl+Y" },
        { L"Duplicate\tCtrl+D", L"Dupliraj\tCtrl+D", L"Duplizieren\tStrg+D", L"Dupliquer\tCtrl+D", L"Duplicar\tCtrl+D", L"Duplica\tCtrl+D", L"Дублировать\tCtrl+D", L"Dupliciraj\tCtrl+D" },
        { L"Lock\tCtrl+L", L"Zaključaj\tCtrl+L", L"Sperren\tStrg+L", L"Verrouiller\tCtrl+L", L"Bloquear\tCtrl+L", L"Blocca\tCtrl+L", L"Заблокировать\tCtrl+L", L"Zaključaj\tCtrl+L" },
        { L"Unlock\tCtrl+L", L"Otključaj\tCtrl+L", L"Entsperren\tStrg+L", L"Déverrouiller\tCtrl+L", L"Desbloquear\tCtrl+L", L"Sblocca\tCtrl+L", L"Разблокировать\tCtrl+L", L"Otključaj\tCtrl+L" },
        { L"Delete\tDel", L"Obriši\tDel", L"Löschen\tEntf", L"Supprimer\tSuppr", L"Eliminar\tSupr", L"Elimina\tCanc", L"Удалить\tDel", L"Obriši\tDel" },
        { L"Bring forward\tCtrl+]", L"Pomeri napred\tCtrl+]", L"Nach vorne\tStrg+]", L"Avancer\tCtrl+]", L"Traer adelante\tCtrl+]", L"Porta avanti\tCtrl+]", L"На передний план\tCtrl+]", L"Pomakni naprijed\tCtrl+]" },
        { L"Send backward\tCtrl+[", L"Pomeri nazad\tCtrl+[", L"Nach hinten\tStrg+[", L"Reculer\tCtrl+[", L"Enviar atrás\tCtrl+[", L"Porta indietro\tCtrl+[", L"На задний план\tCtrl+[", L"Pomakni natrag\tCtrl+[" },
        { L"Bring to front\tCtrl+Shift+]", L"Na vrh\tCtrl+Shift+]", L"In den Vordergrund\tStrg+Umschalt+]", L"Premier plan\tCtrl+Maj+]", L"Traer al frente\tCtrl+Mayús+]", L"Porta in primo piano\tCtrl+Maiusc+]", L"На самый верх\tCtrl+Shift+]", L"Na vrh\tCtrl+Shift+]" },
        { L"Send to back\tCtrl+Shift+[", L"Na dno\tCtrl+Shift+[", L"In den Hintergrund\tStrg+Umschalt+[", L"Arrière-plan\tCtrl+Maj+[", L"Enviar al fondo\tCtrl+Mayús+[", L"Porta in secondo piano\tCtrl+Maiusc+[", L"На самый низ\tCtrl+Shift+[", L"Na dno\tCtrl+Shift+[" },
        { L"Rotate left\tQ", L"Rotiraj levo\tQ", L"Nach links drehen\tQ", L"Rotation gauche\tQ", L"Rotar izquierda\tQ", L"Ruota a sinistra\tQ", L"Повернуть влево\tQ", L"Rotiraj lijevo\tQ" },
        { L"Rotate right\tE", L"Rotiraj desno\tE", L"Nach rechts drehen\tE", L"Rotation droite\tE", L"Rotar derecha\tE", L"Ruota a destra\tE", L"Повернуть вправо\tE", L"Rotiraj desno\tE" },
        { L"Nudge\tArrow keys", L"Pomeraj\tStrelice", L"Verschieben\tPfeiltasten", L"Déplacer\tFlèches", L"Mover\tFlechas", L"Sposta\tFrecce", L"Сдвиг\tСтрелки", L"Pomakni\tStrelice" },
        { L"Cycle selection\tTab / Shift+Tab", L"Sledeći element\tTab / Shift+Tab", L"Auswahl wechseln\tTab / Umschalt+Tab", L"Parcourir la sélection\tTab / Maj+Tab", L"Ciclar selección\tTab / Mayús+Tab", L"Cicla selezione\tTab / Maiusc+Tab", L"Цикл выбора\tTab / Shift+Tab", L"Ciklički odabir\tTab / Shift+Tab" },
        { L"Disable snap while dragging\tAlt", L"Isključi snap tokom prevlačenja\tAlt", L"Einrasten beim Ziehen deaktivieren\tAlt", L"Désactiver l'aimantation en faisant glisser\tAlt", L"Desactivar ajuste al arrastrar\tAlt", L"Disattiva snap durante il trascinamento\tAlt", L"Отключить привязку при перетаскивании\tAlt", L"Isključi snap tijekom povlačenja\tAlt" },
        { L"Commit text\tCtrl+Enter", L"Potvrdi tekst\tCtrl+Enter", L"Text übernehmen\tStrg+Enter", L"Valider le texte\tCtrl+Entrée", L"Confirmar texto\tCtrl+Intro", L"Conferma testo\tCtrl+Invio", L"Применить текст\tCtrl+Enter", L"Potvrdi tekst\tCtrl+Enter" },
        { L"Cancel capture\tEsc", L"Otkaži snimanje\tEsc", L"Aufnahme abbrechen\tEsc", L"Annuler la capture\tÉchap", L"Cancelar captura\tEsc", L"Annulla cattura\tEsc", L"Отменить захват\tEsc", L"Odustani od snimanja\tEsc" },
        { L"Text  (drag to move)", L"Tekst  (prevuci za pomeranje)", L"Text  (ziehen zum Verschieben)", L"Texte  (glisser pour déplacer)", L"Texto  (arrastrar para mover)", L"Testo  (trascina per spostare)", L"Текст  (перетащите для перемещения)", L"Tekst  (povuci za pomicanje)" },
        { L"About Quickshot", L"O programu Quickshot", L"Über Quickshot", L"À propos de Quickshot", L"Acerca de Quickshot", L"Informazioni su Quickshot", L"О программе Quickshot", L"O programu Quickshot" },
        { L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot", L"Quickshot" },
        { L"Version %s", L"Verzija %s", L"Version %s", L"Version %s", L"Versión %s", L"Versione %s", L"Версия %s", L"Verzija %s" },
        { L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic", L"Copyright (c) 2026 Ivan Stojmenovic" },
        { L"Author: Ivan Stojmenovic", L"Autor: Ivan Stojmenovic", L"Autor: Ivan Stojmenovic", L"Auteur : Ivan Stojmenovic", L"Autor: Ivan Stojmenovic", L"Autore: Ivan Stojmenovic", L"Автор: Ivan Stojmenovic", L"Autor: Ivan Stojmenovic" },
        { L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com", L"office.stojmenovic@gmail.com" },
        { L"Check for updates...", L"Proveri ažuriranja...", L"Nach Updates suchen...", L"Rechercher des mises à jour...", L"Buscar actualizaciones...", L"Controlla aggiornamenti...", L"Проверить обновления...", L"Provjeri ažuriranja..." },
        { L"Check for updates...", L"Proveri ažuriranja...", L"Nach Updates suchen...", L"Rechercher des mises à jour...", L"Buscar actualizaciones...", L"Controlla aggiornamenti...", L"Проверить обновления...", L"Provjeri ažuriranja..." },
        { L"Update", L"Ažuriranje", L"Update", L"Mise à jour", L"Actualización", L"Aggiornamento", L"Обновление", L"Ažuriranje" },
        { L"Set the update manifest URL in Options.", L"Podesi URL update manifesta u Opcijama.", L"Legen Sie die Update-Manifest-URL in den Optionen fest.", L"Définissez l'URL du manifeste de mise à jour dans Options.", L"Configura la URL del manifiesto en Opciones.", L"Imposta l'URL del manifesto aggiornamenti in Opzioni.", L"Укажите URL манифеста обновлений в параметрах.", L"Postavi URL update manifesta u Opcijama." },
        { L"Invalid update URL.", L"Neispravan update URL.", L"Ungültige Update-URL.", L"URL de mise à jour non valide.", L"URL de actualización no válida.", L"URL di aggiornamento non valida.", L"Неверный URL обновления.", L"Neispravan update URL." },
        { L"Update manifest is invalid.", L"Update manifest nije ispravan.", L"Update-Manifest ist ungültig.", L"Le manifeste de mise à jour est invalide.", L"El manifiesto de actualización no es válido.", L"Il manifesto di aggiornamento non è valido.", L"Манифест обновления недействителен.", L"Update manifest nije ispravan." },
        { L"Update check failed.", L"Provera ažuriranja nije uspela.", L"Update-Prüfung fehlgeschlagen.", L"Échec de la vérification des mises à jour.", L"Error al buscar actualizaciones.", L"Controllo aggiornamenti non riuscito.", L"Не удалось проверить обновления.", L"Provjera ažuriranja nije uspjela." },
        { L"You already have the latest version.", L"Već imaš najnoviju verziju.", L"Sie haben bereits die neueste Version.", L"Vous avez déjà la dernière version.", L"Ya tienes la última versión.", L"Hai già l'ultima versione.", L"У вас уже последняя версия.", L"Već imaš najnoviju verziju." },
        { L"A new version is available.", L"Dostupna je nova verzija.", L"Eine neue Version ist verfügbar.", L"Une nouvelle version est disponible.", L"Hay una nueva versión disponible.", L"È disponibile una nuova versione.", L"Доступна новая версия.", L"Dostupna je nova verzija." },
        { L"Current:", L"Trenutna:", L"Aktuell:", L"Actuelle :", L"Actual:", L"Attuale:", L"Текущая:", L"Trenutna:" },
        { L"New:", L"Nova:", L"Neu:", L"Nouvelle :", L"Nueva:", L"Nuova:", L"Новая:", L"Nova:" },
        { L"Download and install now?", L"Preuzeti i instalirati sada?", L"Jetzt herunterladen und installieren?", L"Télécharger et installer maintenant ?", L"¿Descargar e instalar ahora?", L"Scaricare e installare ora?", L"Скачать и установить сейчас?", L"Preuzeti i instalirati sada?" },
        { L"Update downloaded. The app will restart.", L"Ažuriranje preuzeto. Aplikacija će se restartovati.", L"Update heruntergeladen. Die App wird neu gestartet.", L"Mise à jour téléchargée. L'application va redémarrer.", L"Actualización descargada. La aplicación se reiniciará.", L"Aggiornamento scaricato. L'app verrà riavviata.", L"Обновление загружено. Приложение перезапустится.", L"Ažuriranje preuzeto. Aplikacija će se ponovno pokrenuti." },
        { L"Check for updates on startup", L"Proveri ažuriranja pri pokretanju", L"Beim Start nach Updates suchen", L"Rechercher les mises à jour au démarrage", L"Buscar actualizaciones al iniciar", L"Controlla aggiornamenti all'avvio", L"Проверять обновления при запуске", L"Provjeri ažuriranja pri pokretanju" },
        { L"Update manifest URL:", L"URL update manifesta:", L"Update-Manifest-URL:", L"URL du manifeste de mise à jour :", L"URL del manifiesto de actualización:", L"URL manifesto aggiornamenti:", L"URL манифеста обновлений:", L"URL update manifesta:" },
    };
}

void Loc::SetLanguage(UiLang lang)
{
    if (lang < UiLang::En || lang >= UiLang::Count)
        lang = UiLang::En;
    g_lang = lang;
}

UiLang Loc::GetLanguage() { return g_lang; }

UiLang Loc::FromIndex(int index)
{
    if (index < 0 || index >= kLangCount) return UiLang::En;
    return static_cast<UiLang>(index);
}

int Loc::ToIndex(UiLang lang)
{
    const int i = static_cast<int>(lang);
    return (i >= 0 && i < kLangCount) ? i : 0;
}

int Loc::LanguageCount() { return kLangCount; }

const wchar_t* Loc::LanguageDisplayName(int index)
{
    if (index < 0 || index >= kLangCount) return kLangNames[0];
    return kLangNames[index];
}

const wchar_t* Loc::T(StrId id)
{
    const size_t i = static_cast<size_t>(id);
    if (i >= static_cast<size_t>(StrId::Count)) return L"";
    const int li = ToIndex(g_lang);
    const wchar_t* s = kStrings[i].t[li];
    return s ? s : kStrings[i].t[0];
}
