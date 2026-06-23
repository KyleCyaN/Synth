#include "InjectorGUI.h"
#include <QFileDialog>
#include <windows.h>
#include <tlhelp32.h>
#include <qcoreapplication.h>
#include "core/Inject.h"
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QResizeEvent>
#include <QApplication>
#include <QFile>
#include <QDir>

/*
std::string GetSystemUUID()
{
    const DWORD tableType = 'RSMB';
    DWORD size = GetSystemFirmwareTable(tableType, 0, nullptr, 0);
    if (!size) return {};

    std::vector<BYTE> buf(size);
    if (GetSystemFirmwareTable(tableType, 0, buf.data(), size) != size)
        return {};

    BYTE* p = buf.data();
    BYTE* end = p + size;

    while (p < end)
    {
        if (p[0] == 1 && p[1] >= 24)
        {
            char uuid[37]{0};
            snprintf(uuid, sizeof(uuid),
                "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                p[8], p[9], p[10], p[11],
                p[12], p[13], p[14], p[15],
                p[16], p[17], p[18], p[19], p[20], p[21], p[22], p[23]);
            return uuid;
        }
        p += p[1];
        while (p < end && (*p || *(p + 1))) p++;
        p += 2;
    }
    return {};
}
*/

/*
bool IsUuidAllowed(const std::string& uuid)
{
    static const std::vector<std::string> whitelist = {
        "575B910D-D542-572D-81EF-60CF84C6FAFB",
        "ECB56A82-88DC-8DD2-8363-3C7C3FD78A58",
        "31C0AC77-054B-8041-A58A-303FC1DEB048"
    };

    return std::find(whitelist.begin(), whitelist.end(), uuid) != whitelist.end();
}
*/

/*
void ShowUuidDeniedAndCopy()
{
    std::string uuid = GetSystemUUID();

    QString text =
        "❌ Authorization failed.\n\n"
        "The current device is not on the whitelist。\n\n"
        "Please click OK to automatically copy the UUID below and send it to the dev：\n\n" +
        QString::fromStdString(uuid);

    QMessageBox msgBox;
    msgBox.setWindowTitle("Authorization failed");
    msgBox.setText(text);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();

    if (QApplication::clipboard())
    {
        QApplication::clipboard()->setText(QString::fromStdString(uuid));
    }
}
*/

InjectorGUI::InjectorGUI(QWidget *parent)
    : QWidget(parent)
    , targetProcessId(0)
    , processFound(false)
    , hInjectedModule(nullptr)
{
    targetProcessName = "WindowsEntryPoint.Windows_W10.exe";
    dllName= "Synth.dll";
    dllPath = QCoreApplication::applicationDirPath() + "/" + dllName;

    setupUI();

    processCheckTimer = new QTimer(this);
    connect(processCheckTimer, &QTimer::timeout, this, &InjectorGUI::onCheckProcess);
    processCheckTimer->start(2000);

    onCheckProcess();
}

InjectorGUI::~InjectorGUI()
{
    if (hInjectedModule) {
        hInjectedModule = nullptr;
    }
}

void InjectorGUI::setupUI()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(420, 260);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *titleBar = new QWidget(this);
    titleBar->setFixedHeight(40);

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 10, 0);

    QLabel *title = new QLabel("SYNTH", this);
    title->setStyleSheet("color: white; font-size: 14px;");

    QPushButton *btnMin = new QPushButton("-", this);
    QPushButton *btnClose = new QPushButton("X", this);

    btnMin->setFixedSize(30, 25);
    btnClose->setFixedSize(30, 25);

    titleLayout->addWidget(title);
    titleLayout->addStretch();
    titleLayout->addWidget(btnMin);
    titleLayout->addWidget(btnClose);

    QWidget *content = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(content);

    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    statusLabel = new QLabel("Status: Checking...", this);
    statusLabel->setAlignment(Qt::AlignCenter);

    pidLabel = new QLabel("PID: 404 Not Found", this);
    pidLabel->setAlignment(Qt::AlignCenter);

    injectButton = new QPushButton("Inject", this);
    unloadButton = new QPushButton("Unload", this);

    injectButton->setFixedHeight(40);
    unloadButton->setFixedHeight(40);

    injectButton->setEnabled(false);
    unloadButton->setEnabled(false);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);
    btnLayout->addWidget(injectButton);
    btnLayout->addWidget(unloadButton);

    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(pidLabel);
    mainLayout->addStretch();
    mainLayout->addLayout(btnLayout);
    rootLayout->addWidget(titleBar);
    rootLayout->addWidget(content);

    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    connect(btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(injectButton, &QPushButton::clicked, this, &InjectorGUI::onInjectClicked);
    connect(unloadButton, &QPushButton::clicked, this, &InjectorGUI::onUnloadClicked);

    setStyleSheet(R"(
        * { font-family: "Microsoft YaHei", "Segoe UI"; }

        QWidget {
            background-color: #292f44;
        }

        QLabel {
            color: #E0E0E0;
            font-size: 16px;
        }

        QPushButton {
            background-color: #1f1f1f;
            color: #00FFCC;
            border: 1px solid #00FFCC;
            border-radius: 6px;
            font-size: 16px
        }

        QPushButton:hover {
            background-color: #00FFCC;
            color: black;
        }

        QPushButton:disabled {
            color: #555;
            border: 1px solid #555;
        }
    )");
}

void InjectorGUI::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragPosition = event->globalPos() - frameGeometry().topLeft();
    }
}

void InjectorGUI::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - dragPosition);
    }
}

void InjectorGUI::mouseReleaseEvent(QMouseEvent *event)
{
    dragging = false;
}

void InjectorGUI::onCheckProcess()
{
    DWORD currentProcessId = Inject::GetProcId(targetProcessName.toUtf8().constData());
    bool processExists = (currentProcessId != 0);

    if (!processExists) {
        processFound = false;
        targetProcessId = 0;
        hInjectedModule = nullptr;

        statusLabel->setText("Status: ❌ Game process NOT FOUND");
        statusLabel->setStyleSheet("QLabel { font-size: 16px; color: red; padding: 10px; }");
        pidLabel->setText("PID: 404 NOT FOUND");

        injectButton->setEnabled(false);
        unloadButton->setEnabled(false);
        injectButton->setText("Inject");
        return;
    }

    targetProcessId = currentProcessId;
    processFound = true;

    statusLabel->setText("Status: ✅ Game process FOUND");
    statusLabel->setStyleSheet("QLabel { font-size: 16px; color: green; padding: 10px; }");
    pidLabel->setText(QString("PID: %1").arg(currentProcessId));

    if (hInjectedModule != nullptr) {
        injectButton->setEnabled(false);
        unloadButton->setEnabled(true);
        injectButton->setText("Injected");
        statusLabel->setText("Status: ✅ Game process FOUND (Injected)");
    } else {
        injectButton->setEnabled(true);
        unloadButton->setEnabled(false);
        injectButton->setText("Inject");
        statusLabel->setText("Status: ✅ Game process FOUND");
    }
}

bool InjectorGUI::findUWPProcess()
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 processEntry = { sizeof(PROCESSENTRY32) };
    bool found = false;

    if (Process32First(snapshot, &processEntry)) {
        do {
            std::wstring targetWName = targetProcessName.toStdWString();
            if (wcscmp(processEntry.szExeFile, targetWName.c_str()) == 0) {
                targetProcessId = processEntry.th32ProcessID;
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return found;
}

void InjectorGUI::onInjectClicked()
{
    /*
    std::string uuid = GetSystemUUID();


    if (!IsUuidAllowed(uuid))
    {
        ShowUuidDeniedAndCopy();
        return;
    }
    */

    DWORD processId = Inject::GetProcId(targetProcessName.toUtf8().constData());
    if (processId == 0) {
        QMessageBox::warning(this, "Error", "Game process NOT FOUND");
        return;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);
        if (Module32First(hSnapshot, &me32)) {
            do {
                std::wstring moduleName = me32.szModule;
                if (moduleName.find(L"Synth") != std::wstring::npos) {
                    QMessageBox::information(this, "Notice", "DLL has been injected！");
                    CloseHandle(hSnapshot);
                    injectButton->setEnabled(false);
                    unloadButton->setEnabled(true);
                    injectButton->setText("Injected");
                    return;
                }
            } while (Module32Next(hSnapshot, &me32));
        }
        CloseHandle(hSnapshot);
    }

    HANDLE moduleHandle = nullptr;
    bool result = Inject::inject(processId, dllPath, moduleHandle);

    if (result) {
        hInjectedModule = moduleHandle;
        injectButton->setEnabled(false);
        unloadButton->setEnabled(true);
        injectButton->setText("Injected");
    } else {
        hInjectedModule = nullptr;
        DWORD lastError = GetLastError();
        QString errorDetail;
        switch(lastError) {
            case 5:  errorDetail = "ERROR_ACCESS_DENIED\nPlease run this program as an administrator"; break;
            case 6:  errorDetail = "ERROR_INVALID_HANDLE\nThe target process may have exited"; break;
            case 299: errorDetail = "ERROR_PARTIAL_COPY\nMemory read/write operation failed, possibly blocked by antivirus software"; break;
            default: errorDetail = QString("Error code: %1").arg(lastError); break;
        }

        QMessageBox::critical(this, "Error",
            QString("Inject failed！\n\n").append(errorDetail));
    }
}

void InjectorGUI::onUnloadClicked()
{
    DWORD processId = Inject::GetProcId(targetProcessName.toUtf8().constData());
    if (processId == 0) {
        hInjectedModule = nullptr;
        unloadButton->setEnabled(false);
        injectButton->setEnabled(false);
        injectButton->setText("Inject");
        QMessageBox::warning(this, "Notice", "Game process has been killed, no need to unload.");
        return;
    }

    if (bool result = Inject::unload()) {
        hInjectedModule = nullptr;
        unloadButton->setEnabled(false);
        injectButton->setEnabled(true);
        injectButton->setText("Inject");
        QMessageBox::information(this, "Success", "Unload successfully！");
    } else {
        QMessageBox::critical(this, "Failed", "Unload failed!");
    }
}

bool InjectorGUI::injectDLL(DWORD processId, const QString& dllPath)
{
    return Inject::inject(processId, dllPath, hInjectedModule);
}

bool InjectorGUI::unloadDLL(DWORD processId, const QString& dllName)
{
    return Inject::unload();
}