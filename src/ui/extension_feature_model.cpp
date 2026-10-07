/**
 * @file extension_feature_model.cpp
 * @brief Implementation of extension feature registry and status discovery.
 */

#include "extension_feature_model.h"
#include "core/build_config.h"
#include "core/package_manager_helper.h"
#include <QStandardPaths>
#include <QFile>

QList<ExtensionModuleInfo> ExtensionFeatureRegistry::allModules()
{
    QList<ExtensionModuleInfo> list;

    // 0. Poppler Qt6 Core Engine (PDF Engine)
    {
        ExtensionModuleInfo m;
        m.id = "engine-poppler";
        m.displayName = tr("Poppler Qt6 Document Engine");
        m.systemPackage = "libpoppler-qt6-3";
        m.category = "PDF";
        m.isEngine = true;
        m.summary = tr("Core vector and bitmap PDF rendering pipeline, text selection, and document navigation.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticPdf();
        m.diagnosticCommand = "pkg-config --modversion poppler-qt6 2>/dev/null || echo 'poppler-qt6 present'";
        m.features = {
            {
                tr("High-Resolution Vector PDF Rendering"),
                tr("Sub-pixel text and vector antialiasing with hardware-accelerated tile rasterization."),
                "PdfViewer::renderPage()",
                tr("Integrated core PDF viewer"),
                true
            },
            {
                tr("Document Navigation & Outline"),
                tr("Parses table of contents, bookmarks, internal hyperlinks, and page annotations."),
                "Poppler::Document::load()",
                tr("Document Index / Bookmarks Panel"),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 1. pdfimages (Poppler Utilities)
    {
        ExtensionModuleInfo m;
        m.id = "pdfimages";
        m.displayName = tr("Embedded Image Extraction (pdfimages)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("pdfimages");
        m.category = "PDF";
        m.summary = tr("High-speed lossless dump of raster streams and photographs embedded inside PDF files.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticPdf();
        m.diagnosticCommand = "pdfimages -v";
        m.features = {
            {
                tr("Lossless Embedded Image Extraction"),
                tr("Exports raw image streams directly from PDF object tables without re-compression loss."),
                "PdfViewer::extractImages()",
                tr("Menu: Tools → Extract Images... (Ctrl+Shift+E)"),
                true
            },
            {
                tr("Fast Thumbnail Generation"),
                tr("Accelerates multi-page grid rendering by extracting embedded preview bitmaps when present."),
                "PdfDocumentOps::extractImages()",
                tr("Page Organizer Dialog / Thumbnails Panel"),
                false
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 2. poppler-data (International & CJK Fonts)
    {
        ExtensionModuleInfo m;
        m.id = "poppler-data";
        m.displayName = tr("International & Asian Font Support (poppler-data)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("poppler-data");
        m.category = "PDF";
        m.summary = tr("CMAP character encoding tables for rendering Chinese, Japanese, Korean, and non-Western glyphs.");
        m.canCompile = false;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "ls -d /usr/share/poppler/cMap";
        m.features = {
            {
                tr("CJK Character Rendering"),
                tr("Enables correct text rendering and selection for Asian (Chinese, Japanese, Korean) documents."),
                "Poppler::Page::renderToImage()",
                tr("Integrated silently into all PDF viewing tabs"),
                true
            },
            {
                tr("International Full-Text Search"),
                tr("Normalizes multilingual Unicode mappings for cross-page text search and highlighting."),
                "PdfViewer::searchForward()",
                tr("Search Bar (Ctrl+F)"),
                false
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 3. QPDF (Optimization & Repair)
    {
        ExtensionModuleInfo m;
        m.id = "qpdf";
        m.displayName = tr("Advanced PDF Tools & Optimization (QPDF)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("qpdf");
        m.category = "PDF";
        m.summary = tr("Linearization, structural decompression, page assembly, and PDF integrity repairs.");
        m.canCompile = true;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "qpdf --version";
        m.features = {
            {
                tr("Linearize PDF (Fast Web View)"),
                tr("Reorders internal dictionaries so the first page renders instantly before full file downloads."),
                "PdfOptimizer::linearizeFile(const QString &path)",
                tr("Menu: Tools → Optimize PDF..."),
                true
            },
            {
                tr("Corrupt Stream Repair & Recovery"),
                tr("Rebuilds broken cross-reference (XRef) tables and damaged object streams."),
                "PdfRepairDialog::startRepair()",
                tr("Menu: Tools → Repair Damaged PDF..."),
                true
            },
            {
                tr("Lossless Structural Compression"),
                tr("Deduplicates repeated fonts and compacts uncompressed object streams to shrink file size."),
                "PdfOptimizer::compressStreams()",
                tr("Save As Dialog → 'Optimize file structure'"),
                false
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 4. Tesseract OCR
    {
        ExtensionModuleInfo m;
        m.id = "tesseract";
        m.displayName = tr("Optical Text Recognition (Tesseract OCR)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("tesseract");
        m.category = "PDF";
        m.summary = tr("Neural-network powered character recognition for scanned PDF pages and images.");
        m.canCompile = true;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "tesseract --version";

        // Sub-dependencies (Italian language data pack)
        bool hasIta = QFile::exists("/usr/share/tesseract-ocr/5/tessdata/ita.traineddata") ||
                      QFile::exists("/usr/share/tesseract-ocr/4.00/tessdata/ita.traineddata") ||
                      QFile::exists("/usr/share/tessdata/ita.traineddata");
        m.subDependencies.append(SubDependency{
            tr("Italian Language Pack"),
            "tesseract-ocr-ita",
            hasIta,
            tr("Enables Italian dictionary recognition for high-accuracy OCR results.")
        });

        m.features = {
            {
                tr("Searchable PDF Generation"),
                tr("Embeds a transparent, selectable UTF-8 text layer beneath scanned page graphics."),
                "OcrEngine::processDocument()",
                tr("Menu: Tools → Run OCR on Document..."),
                true
            },
            {
                tr("Marquee Area Snipping to Clipboard"),
                tr("Select any visual region on screen and copy recognized plain text straight to clipboard."),
                "OcrSnipperOverlay::captureAndRecognize()",
                tr("Right-click context menu → 'Copy Selected Text via OCR'"),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 5. OpenSSL Digital Signatures
    {
        ExtensionModuleInfo m;
        m.id = "openssl";
        m.displayName = tr("Cryptographic Digital Signatures (OpenSSL)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("openssl");
        m.category = "PDF";
        m.summary = tr("PAdES and PKCS#12 cryptographic verification and X.509 digital signing of PDF files.");
        m.canCompile = true;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "openssl version";
        m.features = {
            {
                tr("Digital Signature Verification"),
                tr("Validates PAdES signatures, checks signer certificate chain, and detects file tampering."),
                "PdfSignatureDialog::verifySignatures()",
                tr("Document Status Bar / Signature Panel"),
                true
            },
            {
                tr("Cryptographic Document Signing"),
                tr("Applies visual and cryptographic signatures to PDFs using personal .p12/.pfx certificates."),
                "PdfSignatureDialog::signDocument()",
                tr("Menu: Tools → Digitally Sign Document..."),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // LibArchive Universal Engine
    {
        ExtensionModuleInfo m;
        m.id = "engine-libarchive";
        m.displayName = tr("LibArchive Universal Compression Engine");
        m.systemPackage = "libarchive13";
        m.category = "Archive";
        m.isEngine = true;
        m.summary = tr("Universal container streaming parser for ZIP, 7-Zip, TAR, CPIO, ISO, and multi-part archives.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticArchive();
        m.diagnosticCommand = "pkg-config --modversion libarchive 2>/dev/null || echo 'libarchive present'";
        m.features = {
            {
                tr("Multi-Format Archive Streaming"),
                tr("Instant cataloging and zero-copy direct extraction of archive entries and compressed files."),
                "ArchiveViewer::openArchive()",
                tr("Archive Explorer Tab"),
                true
            },
            {
                tr("Archive Creation & Packing"),
                tr("Creates new compressed containers with customizable compression algorithms and levels."),
                "ArchiveViewer::compressFolder()",
                tr("Menu: File → Compress to Archive..."),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 6. Zstandard (zstd)
    {
        ExtensionModuleInfo m;
        m.id = "zstd";
        m.displayName = tr("Zstandard High-Speed Codec (zstd)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("zstd");
        m.category = "Archive";
        m.summary = tr("Next-generation real-time compression algorithm developed by Meta for high throughput.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticArchive();
        m.diagnosticCommand = "zstd --version";
        m.features = {
            {
                tr("TAR.ZST & ZST Archive Browsing"),
                tr("Open, inspect, and extract modern .tar.zst archives at multi-gigabyte per second speeds."),
                "ArchiveViewer::openArchive()",
                tr("Archive Explorer Tab"),
                true
            },
            {
                tr("Multi-threaded Compression"),
                tr("Parallelized compression utilizing all available CPU cores without throttling system responsiveness."),
                "ArchiveViewer::compressFolder()",
                tr("Menu: File → Compress to Archive..."),
                false
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 7. LZMA / XZ
    {
        ExtensionModuleInfo m;
        m.id = "xz";
        m.displayName = tr("LZMA Maximum Compression Codec (xz)");
        m.systemPackage = PackageManagerHelper::packageNameForComponent("xz");
        m.category = "Archive";
        m.summary = tr("High-ratio compression format widely adopted across Linux package distributions and source tarballs.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticArchive();
        m.diagnosticCommand = "xz --version";
        m.features = {
            {
                tr("TAR.XZ & XZ Archive Decompression"),
                tr("Full read and write support for .tar.xz, .txz, and standalone .xz compressed streams."),
                "ArchiveViewer::openArchive()",
                tr("Archive Explorer Tab"),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // Internal Office Parser Engine
    {
        ExtensionModuleInfo m;
        m.id = "engine-office";
        m.displayName = tr("Internal Office Document Parser Engine");
        m.systemPackage = "librevenge-0.0-0";
        m.category = "Office";
        m.isEngine = true;
        m.summary = tr("Embedded structured parser for Microsoft Office and OpenDocument files without external LibreOffice.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticOffice();
        m.diagnosticCommand = "pkg-config --modversion librevenge-0.0 2>/dev/null || echo 'librevenge present'";
        m.features = {
            {
                tr("Native DOCX / ODT Parsing"),
                tr("Fast standalone formatting parsing directly into Qt rich-text documents without background daemons."),
                "OfficeViewer::loadDocument()",
                tr("Office Viewer Tab"),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 8. LibreOffice / Internal Office Viewer
    {
        ExtensionModuleInfo m;
        m.id = "office";
        m.displayName = tr("Office Document Reader & Converter (LibreOffice)");
        m.systemPackage = "libreoffice";
        m.category = "Office";
        m.summary = tr("Headless rendering and conversion of Microsoft Office (.docx, .xlsx, .pptx) and OpenDocument files.");
        m.canCompile = true;
        m.isStaticCompiled = BuildConfig::instance().staticOffice();
        m.diagnosticCommand = "libreoffice --version";

        bool hasLo = !QStandardPaths::findExecutable("libreoffice").isEmpty();
        m.subDependencies.append(SubDependency{
            tr("LibreOffice Suite"),
            "libreoffice",
            hasLo,
            tr("Provides full-fidelity rendering for complex spreadsheets, charts, and presentations.")
        });

        m.features = {
            {
                tr("Word Document (.docx, .doc) Reading"),
                tr("Instant visual preview and text extraction for Microsoft Word and Writer files."),
                "OfficeViewer::loadDocument()",
                tr("Office Viewer Tab"),
                true
            },
            {
                tr("Excel Spreadsheet (.xlsx, .xls) Preview"),
                tr("Inspect sheets, multi-column tables, and formula results without opening external heavy office suites."),
                "OfficeViewer::renderSpreadsheet()",
                tr("Office Viewer Tab"),
                true
            },
            {
                tr("Headless PDF Export"),
                tr("Batch converts office formats to PDF on-the-fly using background daemon worker."),
                "OfficeViewer::convertViaLibreOffice()",
                tr("Menu: File → Export Office Document as PDF..."),
                false
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // MPV Media Engine
    {
        ExtensionModuleInfo m;
        m.id = "engine-mpv";
        m.displayName = tr("MPV Video & Audio Playback Engine (libmpv)");
        m.systemPackage = "libmpv2";
        m.category = "Media";
        m.isEngine = true;
        m.summary = tr("Hardware-accelerated media engine driving GPU zero-copy video decode and PipeWire/ALSA audio.");
        m.canCompile = false;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "pkg-config --modversion mpv 2>/dev/null || echo 'libmpv present'";
        m.features = {
            {
                tr("Hardware GPU Video Decoding"),
                tr("Zero-copy hardware surface rendering via VA-API, NVDEC, and VDPAU at 4K/60+ FPS."),
                "MpvVideoWidget::initializeMpv()",
                tr("Media Viewer Tab"),
                true
            },
            {
                tr("High-Fidelity Audio Streaming"),
                tr("Bit-perfect low-latency PCM playback through PipeWire, PulseAudio, and native ALSA."),
                "MediaViewer::openFile()",
                tr("Audio Player Controls"),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 9. FFTW3 (Fast Fourier Transform & Audio Spectrum Analysis)
    {
        ExtensionModuleInfo m;
        m.id = "fftw3";
        m.displayName = tr("Vectorized FFT & Audio Spectrum Engine (FFTW3)");
        m.systemPackage = "libfftw3-3";
        m.category = "Media";
        m.summary = tr("High-performance mathematical DSP engine computing 60 FPS real-time stereo oscilloscope and spectrum bars.");
        m.canCompile = true;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "echo 'FFTW3 DSP Engine ready'";
        m.features = {
            {
                tr("Real-time Stereo Vector Oscilloscope"),
                tr("Computes high-frequency sound wave trajectories across left/right channels at near-zero CPU load."),
                "StereoOscilloscopeWidget::renderFrame()",
                tr("Media Player → Visualizer Tab"),
                true
            },
            {
                tr("Spectrum Equalizer Bar Analysis"),
                tr("Transforms raw PCM audio stream into 32-band frequency analysis with AVX2 vectorization."),
                "AudioProcessor::computeFft()",
                tr("Media Player → Spectrum Mode"),
                false
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    // 10. TagLib (Audio Metadata & Cover Art)
    {
        ExtensionModuleInfo m;
        m.id = "taglib";
        m.displayName = tr("Audio Tag & Embedded Cover Art Engine (TagLib)");
        m.systemPackage = "libtag1v5";
        m.category = "Media";
        m.summary = tr("Ultra-fast parser for ID3v2, Vorbis Comment, FLAC, and APE audio metadata and album art extraction.");
        m.canCompile = true;
        m.isStaticCompiled = false;
        m.diagnosticCommand = "echo 'TagLib Engine ready'";
        m.features = {
            {
                tr("Embedded Album Art Extraction"),
                tr("Reads embedded front-cover JPEG/PNG bytes without decoding the full audio track."),
                "AudioProcessor::readMetadata()",
                tr("Audio File Thumbnails & Info Panel"),
                true
            },
            {
                tr("Batch Audio Tag Editing"),
                tr("Directly modifies song titles, artist, album, and release year in MP3/FLAC/M4A files."),
                "AudioTagDialog::saveTags()",
                tr("Menu: Tools → Edit Audio Tags (Ctrl+T)"),
                true
            }
        };
        refreshModuleStatus(m);
        list.append(m);
    }

    return list;
}

QList<ExtensionModuleInfo> ExtensionFeatureRegistry::modulesForCategory(const QString &category)
{
    QList<ExtensionModuleInfo> res;
    for (const auto &mod : allModules()) {
        if (mod.category.compare(category, Qt::CaseInsensitive) == 0) {
            res.append(mod);
        }
    }
    return res;
}

ExtensionModuleInfo ExtensionFeatureRegistry::moduleById(const QString &id)
{
    for (auto mod : allModules()) {
        if (mod.id == id) {
            refreshModuleStatus(mod);
            return mod;
        }
    }
    return ExtensionModuleInfo{};
}

void ExtensionFeatureRegistry::refreshModuleStatus(ExtensionModuleInfo &module)
{
    if (module.id == "engine-poppler") {
        module.isInstalled = BuildConfig::isPkgInstalled("poppler-qt6") || BuildConfig::isPkgInstalled("libpoppler-qt6-3");
    } else if (module.id == "engine-libarchive") {
        module.isInstalled = BuildConfig::isPkgInstalled("libarchive") || BuildConfig::isPkgInstalled("libarchive13");
    } else if (module.id == "engine-mpv") {
        module.isInstalled = BuildConfig::isPkgInstalled("mpv") || BuildConfig::isPkgInstalled("libmpv2") || BuildConfig::isPkgInstalled("libmpv1");
    } else if (module.id == "engine-office") {
        module.isInstalled = BuildConfig::isPkgInstalled("librevenge-0.0") || BuildConfig::instance().enableInternalOffice();
    } else if (module.id == "pdfimages") {
        module.isInstalled = BuildConfig::isPdfimagesAvailable();
    } else if (module.id == "poppler-data") {
        module.isInstalled = BuildConfig::isPopplerDataInstalled();
    } else if (module.id == "qpdf") {
        module.isInstalled = BuildConfig::isQpdfAvailable();
    } else if (module.id == "tesseract") {
        module.isInstalled = BuildConfig::isTesseractAvailable();
    } else if (module.id == "openssl") {
        module.isInstalled = BuildConfig::isPkgInstalled("openssl");
    } else if (module.id == "zstd") {
        module.isInstalled = BuildConfig::isPkgInstalled("libzstd");
    } else if (module.id == "xz") {
        module.isInstalled = BuildConfig::isPkgInstalled("liblzma");
    } else if (module.id == "office") {
        module.isInstalled = !QStandardPaths::findExecutable("libreoffice").isEmpty() ||
                             BuildConfig::instance().enableInternalOffice();
    } else if (module.id == "fftw3") {
        module.isInstalled = BuildConfig::isPkgInstalled("fftw3") || BuildConfig::isPkgInstalled("libfftw3-3");
    } else if (module.id == "taglib") {
        module.isInstalled = BuildConfig::isPkgInstalled("taglib") || BuildConfig::isPkgInstalled("libtag1v5");
    } else {
        module.isInstalled = PackageManagerHelper::isComponentInstalled(module.id);
    }
}
