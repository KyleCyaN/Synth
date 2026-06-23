#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <Windows.h>

class InjectorGUI : public QWidget
{
    Q_OBJECT

public:
    explicit InjectorGUI(QWidget* parent = nullptr);
    ~InjectorGUI() override;

private slots:
    void onCheckProcess();
    void onInjectClicked();
    void onUnloadClicked();

private:
    void setupUI();


    bool findUWPProcess();
    bool injectDLL(DWORD processId, const QString& dllPath);
    static bool unloadDLL(DWORD processId, const QString& dllName);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    bool dragging = false;
    QPoint dragPosition;

    QLabel* statusLabel{};
    QLabel* pidLabel{};
    QLabel* statusIndicator{};
    QPushButton* injectButton{};
    QPushButton* unloadButton{};
    QTimer* processCheckTimer;

    DWORD targetProcessId;
    bool processFound;
    QString targetProcessName;
    QString dllPath;
    QString dllName;

    HANDLE hInjectedModule;
};
