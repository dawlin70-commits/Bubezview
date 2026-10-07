#include "pdf_merge_dialog.h"
#include "build_config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

PdfMergeDialog::PdfMergeDialog(const QStringList &initialFiles, QWidget *parent)
    : QDialog(parent)
    , m_fileListWidget(nullptr)
    , m_editOutput(nullptr)
    , m_process(new QProcess(this))
    , m_usingQpdf(false)
{
    setWindowTitle(tr("Unisci Documenti PDF"));
    resize(640, 480);
    setupUi();

    for (const QString &file : initialFiles) {
        if (QFileInfo::exists(file) && file.endsWith(".pdf", Qt::CaseInsensitive)) {
            m_fileListWidget->addItem(file);
        }
    }

    if (!initialFiles.isEmpty()) {
        QFileInfo first(initialFiles.first());
        m_editOutput->setText(first.dir().absoluteFilePath("PDF_Unito.pdf"));
    }

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PdfMergeDialog::onProcessFinished);
}

PdfMergeDialog::~PdfMergeDialog()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
}

QString PdfMergeDialog::outputFilePath() const
{
    return m_outPath;
}

void PdfMergeDialog::setupUi()
{
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setSpacing(10);
    mainLay->setContentsMargins(16, 16, 16, 16);

    // Engine banner
    m_lblEngineStatus = new QLabel(this);
    m_lblEngineStatus->setWordWrap(true);
    m_lblEngineStatus->setStyleSheet("padding: 8px 12px; border-radius: 6px; font-size: 12px;");
    mainLay->addWidget(m_lblEngineStatus);
    updateEngineBanner();

    // List and manipulation buttons
    auto *listLay = new QHBoxLayout();
    m_fileListWidget = new QListWidget(this);
    m_fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listLay->addWidget(m_fileListWidget, 1);

    auto *btnLay = new QVBoxLayout();
    btnLay->setSpacing(6);

    m_btnAdd = new QPushButton(tr("Aggiungi PDF..."), this);
    m_btnRemove = new QPushButton(tr("Rimuovi"), this);
    m_btnMoveUp = new QPushButton(tr("Sposta Su"), this);
    m_btnMoveDown = new QPushButton(tr("Sposta Giù"), this);
    m_btnClear = new QPushButton(tr("Svuota"), this);

    btnLay->addWidget(m_btnAdd);
    btnLay->addWidget(m_btnRemove);
    btnLay->addWidget(m_btnMoveUp);
    btnLay->addWidget(m_btnMoveDown);
    btnLay->addWidget(m_btnClear);
    btnLay->addStretch();
    listLay->addLayout(btnLay);

    mainLay->addLayout(listLay);

    // Output destination
    auto *outBox = new QHBoxLayout();
    auto *lblOut = new QLabel(tr("Salva come:"), this);
    m_editOutput = new QLineEdit(this);
    m_editOutput->setPlaceholderText(tr("Percorso del file PDF di destinazione..."));
    m_btnBrowseOutput = new QPushButton(tr("Sfoglia..."), this);

    outBox->addWidget(lblOut);
    outBox->addWidget(m_editOutput, 1);
    outBox->addWidget(m_btnBrowseOutput);
    mainLay->addLayout(outBox);

    // Progress and status
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // indeterminate
    m_progressBar->setVisible(false);
    mainLay->addWidget(m_progressBar);

    m_lblStatus = new QLabel(this);
    m_lblStatus->setStyleSheet("color: #64748b; font-size: 11px;");
    mainLay->addWidget(m_lblStatus);

    // Bottom action buttons
    auto *bottomLay = new QHBoxLayout();
    bottomLay->addStretch();

    m_btnClose = new QPushButton(tr("Annulla"), this);
    m_btnMerge = new QPushButton(tr("Unisci Documenti"), this);
    m_btnMerge->setDefault(true);
    m_btnMerge->setStyleSheet("QPushButton { background-color: #0284c7; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px; }"
                              "QPushButton:hover { background-color: #0369a1; }");

    bottomLay->addWidget(m_btnClose);
    bottomLay->addWidget(m_btnMerge);
    mainLay->addLayout(bottomLay);

    // Connect signals
    connect(m_btnAdd, &QPushButton::clicked, this, &PdfMergeDialog::onAddFiles);
    connect(m_btnRemove, &QPushButton::clicked, this, &PdfMergeDialog::onRemoveSelected);
    connect(m_btnMoveUp, &QPushButton::clicked, this, &PdfMergeDialog::onMoveUp);
    connect(m_btnMoveDown, &QPushButton::clicked, this, &PdfMergeDialog::onMoveDown);
    connect(m_btnClear, &QPushButton::clicked, this, &PdfMergeDialog::onClearAll);
    connect(m_btnBrowseOutput, &QPushButton::clicked, this, &PdfMergeDialog::onBrowseOutput);
    connect(m_btnMerge, &QPushButton::clicked, this, &PdfMergeDialog::onStartMerge);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
}

void PdfMergeDialog::updateEngineBanner()
{
    m_usingQpdf = BuildConfig::isQpdfAvailable();
    if (m_usingQpdf) {
        m_lblEngineStatus->setStyleSheet("background-color: rgba(34, 197, 94, 0.15); border: 1px solid rgba(34, 197, 94, 0.4); color: #166534; padding: 8px 12px; border-radius: 6px;");
        m_lblEngineStatus->setText(tr("🟢 <b>Motore QPDF attivo:</b> Unione ottimizzata con deduplicazione automatica dei font, linearizzazione e preservazione intatta di segnalibri e capitoli."));
    } else {
        m_lblEngineStatus->setStyleSheet("background-color: rgba(234, 179, 8, 0.15); border: 1px solid rgba(234, 179, 8, 0.4); color: #854d0e; padding: 8px 12px; border-radius: 6px;");
        m_lblEngineStatus->setText(tr("🟡 <b>Motore Base Poppler attivo (pdfunite):</b> Unione sequenziale delle pagine.<br><span style='font-size: 11px;'>💡 Suggerimento: installa il pacchetto <code>qpdf</code> per preservare segnalibri/capitoli e dimezzare il peso dei font duplicati.</span>"));
    }
}

void PdfMergeDialog::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Seleziona File PDF da Unire"), QString(), tr("Documenti PDF (*.pdf)"));
    for (const QString &f : files) {
        m_fileListWidget->addItem(f);
    }
    if (m_editOutput->text().trimmed().isEmpty() && !files.isEmpty()) {
        QFileInfo first(files.first());
        m_editOutput->setText(first.dir().absoluteFilePath("PDF_Unito.pdf"));
    }
}

void PdfMergeDialog::onRemoveSelected()
{
    qDeleteAll(m_fileListWidget->selectedItems());
}

void PdfMergeDialog::onMoveUp()
{
    int row = m_fileListWidget->currentRow();
    if (row > 0) {
        QListWidgetItem *item = m_fileListWidget->takeItem(row);
        m_fileListWidget->insertItem(row - 1, item);
        m_fileListWidget->setCurrentRow(row - 1);
    }
}

void PdfMergeDialog::onMoveDown()
{
    int row = m_fileListWidget->currentRow();
    if (row >= 0 && row < m_fileListWidget->count() - 1) {
        QListWidgetItem *item = m_fileListWidget->takeItem(row);
        m_fileListWidget->insertItem(row + 1, item);
        m_fileListWidget->setCurrentRow(row + 1);
    }
}

void PdfMergeDialog::onClearAll()
{
    m_fileListWidget->clear();
}

void PdfMergeDialog::onBrowseOutput()
{
    QString defaultPath = m_editOutput->text().trimmed();
    if (defaultPath.isEmpty() && m_fileListWidget->count() > 0) {
        defaultPath = QFileInfo(m_fileListWidget->item(0)->text()).dir().absoluteFilePath("PDF_Unito.pdf");
    }
    QString dest = QFileDialog::getSaveFileName(this, tr("Salva PDF Unito"), defaultPath, tr("Documenti PDF (*.pdf)"));
    if (!dest.isEmpty()) {
        if (!dest.endsWith(".pdf", Qt::CaseInsensitive)) {
            dest += ".pdf";
        }
        m_editOutput->setText(dest);
    }
}

void PdfMergeDialog::onStartMerge()
{
    if (m_fileListWidget->count() < 2) {
        QMessageBox::warning(this, tr("File Insufficienti"), tr("Seleziona almeno 2 file PDF per poter effettuare l'unione."));
        return;
    }

    m_outPath = m_editOutput->text().trimmed();
    if (m_outPath.isEmpty()) {
        onBrowseOutput();
        m_outPath = m_editOutput->text().trimmed();
        if (m_outPath.isEmpty()) return;
    }

    QStringList inputFiles;
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        inputFiles << m_fileListWidget->item(i)->text();
    }

    m_btnMerge->setEnabled(false);
    m_btnAdd->setEnabled(false);
    m_btnRemove->setEnabled(false);
    m_progressBar->setVisible(true);
    m_lblStatus->setText(tr("Unione dei documenti PDF in corso..."));

    QString program;
    QStringList args;

    if (m_usingQpdf) {
        program = "qpdf";
        args << "--empty" << "--pages";
        for (const QString &f : inputFiles) {
            args << f;
        }
        args << "--" << m_outPath;
    } else {
        program = "pdfunite";
        for (const QString &f : inputFiles) {
            args << f;
        }
        args << m_outPath;
    }

    m_process->start(program, args);
    if (!m_process->waitForStarted(2000)) {
        m_progressBar->setVisible(false);
        m_btnMerge->setEnabled(true);
        m_btnAdd->setEnabled(true);
        m_btnRemove->setEnabled(true);
        QMessageBox::critical(this, tr("Errore Avvio"),
            tr("Impossibile avviare il tool di unione '%1'. Assicurati che sia installato nel sistema.").arg(program));
    }
}

void PdfMergeDialog::onProcessFinished(int exitCode)
{
    m_progressBar->setVisible(false);
    m_btnMerge->setEnabled(true);
    m_btnAdd->setEnabled(true);
    m_btnRemove->setEnabled(true);

    if (exitCode == 0 && QFileInfo::exists(m_outPath)) {
        BuildConfig::incrementPdfMergeCount();
        m_lblStatus->setText(tr("Unione completata con successo!"));
        emit fileMerged(m_outPath);

        checkSmartTip();

        QMessageBox::StandardButton res = QMessageBox::information(
            this,
            tr("Unione Riuscita"),
            tr("Il file PDF è stato unito e generato con successo in:<br><b>%1</b><br><br>Vuoi aprire subito il documento in BubezView?").arg(m_outPath),
            QMessageBox::Yes | QMessageBox::No
        );

        if (res == QMessageBox::Yes) {
            accept();
        }
    } else {
        QString err = m_process->readAllStandardError();
        m_lblStatus->setText(tr("Errore durante l'unione."));
        QMessageBox::critical(this, tr("Errore Unione PDF"),
            tr("Si è verificato un errore durante l'unione del PDF:<br><pre>%1</pre>").arg(err));
    }
}

void PdfMergeDialog::checkSmartTip()
{
    if (BuildConfig::pdfMergeCount() >= 5 && !BuildConfig::isQpdfTipDismissed() && !BuildConfig::isQpdfAvailable()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Suggerimento Ottimizzazione PDF"));
        msgBox.setText(tr("<h3>💡 Hai unito %1 documenti PDF con il motore base!</h3>"
                          "<p>BubezView ha notato che utilizzi frequentemente l'unione PDF.<br>"
                          "Ti consigliamo di installare il motore <b>QPDF</b> per:</p>"
                          "<ul>"
                          "<li><b>Deduplicare i font:</b> i file generati occupano molto meno spazio.</li>"
                          "<li><b>Preservare i segnalibri:</b> i capitoli e gli indici originali rimangono intatti e navigabili.</li>"
                          "<li><b>Linearizzazione rapida:</b> apertura istantanea via web e visualizzatori.</li>"
                          "</ul>"
                          "<p>Puoi installarlo subito digitando nel terminale:<br>"
                          "<code>sudo pacman -S qpdf</code> (su Arch/Garuda)<br>"
                          "<code>sudo apt install qpdf</code> (su Debian/Ubuntu)</p>").arg(BuildConfig::pdfMergeCount()));
        
        QPushButton *btnDismiss = msgBox.addButton(tr("Non mostrare più"), QMessageBox::DestructiveRole);
        QPushButton *btnOk = msgBox.addButton(tr("Ho capito"), QMessageBox::AcceptRole);
        Q_UNUSED(btnOk);
        msgBox.exec();

        if (msgBox.clickedButton() == btnDismiss) {
            BuildConfig::dismissQpdfTip();
        }
    }
}
