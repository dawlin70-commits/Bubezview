import xml.etree.ElementTree as ET

translations_map = {
    "Advanced options (diagnostics, details, custom build paths)": "Opzioni avanzate (diagnostica, dettagli tecnici, percorsi custom)",
    "Installed and operational.\nClick to uninstall '%1'.": "Installato ed operativo.\nClicca per disinstallare \"%1\".",
    "Not installed.\nClick to install '%1'.": "Non installato.\nClicca per installare \"%1\".",
    "Native source build active.\nClick to switch to system distribution package.": "Compilato nativo da sorgente attivo.\nClicca per passare al pacchetto di sistema.",
    "System package active.\nClick to switch to native source build mode.": "Pacchetto di sistema attivo.\nClicca per passare alla compilazione nativa da sorgente.",
    "System package active (source build not available for this component).": "Pacchetto di sistema attivo (compilazione da sorgente non disponibile per questo componente).",
    "Uninstall Component": "Disinstalla componente",
    "Do you want to uninstall '%1' from this system?": "Vuoi disinstallare \"%1\" da questo sistema?",
    "🩺 Run Diagnostic Verification...": "🩺 Verifica diagnostica rapida...",
    "🛠️ Check Compiler Prerequisites...": "🛠️ Verifica prerequisiti di compilazione...",
    "🔍 Technical Details && C++ Hooks...": "🔍 Dettagli tecnici && Hook C++...",
    "⚙️ Configure Custom Source && Flags...": "⚙️ Configura sorgente && Flag compilatore...",
    "📋 Copy Component Information": "📋 Copia informazioni componente",
    "Features enabled in BubezView:": "Funzionalità abilitate in BubezView:",
    "Hook": "Hook C++",
    "Menu": "Menu",
    "Sub-dependencies & Language packs:": "Dipendenze secondarie & pacchetti lingua:",
    "Status": "Stato",
    "Present": "Presente",
    "Documents (%1)": "Documenti (%1)",
    "Archives (%1)": "Archivi (%1)",
    "Audio && Video (%1)": "Audio && Video (%1)",
    "Office && Graphics (%1)": "Ufficio && Grafica (%1)",
    "Controls Guide (3 actions per component)": "Guida rapida ai controlli (3 pulsanti per componente)",
    "1. Status / Installation": "1. Stato / Installazione",
    "🟢 Operational  |  ⚪ Not present  |  ⚠️ Issue": "🟢 Operativo  |  ⚪ Non presente  |  ⚠️ Problema",
    "2. Source / Package Switch": "2. Switch Pacchetto / Sorgente",
    "📦 Distro Package  ↔  ⚡ Native Compiled": "📦 Pacchetto distro  ↔  ⚡ Compilato nativo",
    "3. Advanced Options": "3. Opzioni avanzate",
    "··· Single diagnostic, custom paths, logs": "··· Diagnostica singola, percorsi, log",
    "Documents (PDF && Text)": "Documenti (PDF && Testo)",
    "Archives && Compression": "Archivi && Compressione",
    "Office && Graphics": "Ufficio && Grafica"
}

with open("translations/bubezview_it.ts", "r", encoding="utf-8") as f:
    content = f.read()

count = 0
for src, tr in translations_map.items():
    src_xml = src.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("'", "&apos;")
    tr_xml = tr.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("'", "&apos;")
    
    # Try exact match or xml-escaped match
    patterns = [
        f"<source>{src}</source>\n        <translation type=\"unfinished\"></translation>",
        f"<source>{src_xml}</source>\n        <translation type=\"unfinished\"></translation>"
    ]
    replacements = [
        f"<source>{src}</source>\n        <translation>{tr}</translation>",
        f"<source>{src_xml}</source>\n        <translation>{tr_xml}</translation>"
    ]
    
    for pat, rep in zip(patterns, replacements):
        if pat in content:
            content = content.replace(pat, rep)
            count += 1
            break

with open("translations/bubezview_it.ts", "w", encoding="utf-8") as f:
    f.write(content)

print(f"Updated {count} translations in translations/bubezview_it.ts")
