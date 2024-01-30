#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>

namespace Ui {
    class MainWindow;
}

class QStandardItemModel;
enum EChangeType {
    DELETED, ADDED, UPDATED
};

struct SChangedRecord
{
    EChangeType eType;
    int nRecordIndex;
    QByteArray recordContent;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionOpen_triggered();

    void on_actionSave_Database_triggered();

    void on_pbAdd_clicked();

    void on_pbRemove_clicked();

    void on_pbDiff_clicked();

private:
    Ui::MainWindow *ui;

    QStandardItemModel *m_pTableModel;
    QVector<SChangedRecord> m_changedRecords;
};

#endif // MAINWINDOW_H
