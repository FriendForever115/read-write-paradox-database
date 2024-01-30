#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTableView>>

#include "paradox.h"

#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)<(b) ? (a) : (b))

QString g_strDBFileName;
pxhead_t *g_pxh;
pxfield_t *g_pxf;
pxdoc_t *g_pxdoc = NULL;
pxdoc_t *g_pindexdoc = NULL;
pxblob_t *g_pxblob = NULL;

int qsort_len;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_pTableModel = new QStandardItemModel(this);
    ui->tvTableContent->setModel(m_pTableModel);
}

MainWindow::~MainWindow()
{
    delete ui;

    if(g_pxdoc) {
        PX_close(g_pxdoc);
        PX_delete(g_pxdoc);
    }
}

void MainWindow::on_actionOpen_triggered()
{
    m_pTableModel->clear();

    float frecordsize, ffiletype, fprimarykeyfields, ftheonumrecords;
    int recordsize, filetype, primarykeyfields, theonumrecords;
    char *targetencoding = NULL;

    if(NULL == (g_pxdoc = PX_new2(errorhandler, NULL, NULL, NULL))) {
        QMessageBox::information(this, QString("Warning"), QString("Could not create new paradox instance."));
        return;
    }

    g_strDBFileName = QFileDialog::getOpenFileName(this, tr("Open File"),
                                                    NULL,
                                                    tr("DB Files (*.db *.DB)"));

    if(0 > PX_open_file(g_pxdoc, g_strDBFileName.toUtf8().data())) {
        QMessageBox::information(this, QString("Warnning"), QString("Opening database is failed"));
        return;
    }

    /* Open primary index file {{{
    */
    QString strIndexFileName = g_strDBFileName.replace(".db", ".PX");
    strIndexFileName = strIndexFileName.replace(".DB", ".PX");
    if(QFile::exists(strIndexFileName)) {
        g_pindexdoc = PX_new2(errorhandler, NULL, NULL, NULL);
        if(0 > PX_open_file(g_pindexdoc, strIndexFileName.toUtf8().data())) {
            QMessageBox::information(this, QString("Warnning"), QString("Could not open primary index file"));
            return;
        }
        if(0 > PX_read_primary_index(g_pindexdoc)) {
            QMessageBox::information(this, QString("Warnning"), QString("Could not read primary index file"));
            return;
        }
        if(0 > PX_add_primary_index(g_pxdoc, g_pindexdoc)) {
            QMessageBox::information(this, QString("Warnning"), QString("Could not add primary index file"));
            return;
        }
    }
    /* }}} */

    /* Set various variables with values from the header. */
    g_pxh = g_pxdoc->px_head;

    PX_get_value(g_pxdoc, "recordsize", &frecordsize);
    recordsize = (int) frecordsize;

    PX_get_value(g_pxdoc, "filetype", &ffiletype);
    filetype = (int) ffiletype;

    PX_get_value(g_pxdoc, "primarykeyfields", &fprimarykeyfields);
    primarykeyfields = (int) fprimarykeyfields;

    PX_get_value(g_pxdoc, "theonumrecords", &ftheonumrecords);
    theonumrecords = (int) ftheonumrecords;

    if(targetencoding != NULL)
        PX_set_targetencoding(g_pxdoc, targetencoding);

    char *tablename = NULL;
    PX_get_parameter(g_pxdoc, "tablename", &tablename);

    /* Set tablename to the one in the header if it wasn't set before */
    QString strTableName = QString(tablename);
    strTableName.replace('.', '_');
    strTableName.replace(' ', '_');

    ui->lbTableName->setText(strTableName);

    /* Output info {{{
    */
    int nRecordCount = PX_get_num_records(g_pxdoc);
    int nFieldCount = PX_get_num_fields(g_pxdoc);

    float fHeaderSize = 0;
    PX_get_value(g_pxdoc, "headersize", &fHeaderSize);

    float fMaxTableSize = 0;
    PX_get_value(g_pxdoc, "maxtablesize", &fMaxTableSize);

    float fBlocks = 0;
    PX_get_value(g_pxdoc, "numblocks", &fBlocks);

    float fFirstBlock = 0;
    PX_get_value(g_pxdoc, "firstblock", &fFirstBlock);

    float fLastBlock = 0;
    PX_get_value(g_pxdoc, "lastblock", &fLastBlock);

    float fCodePage = 0;
    PX_get_value(g_pxdoc, "codepage", &fCodePage);

    m_pTableModel->setColumnCount(nFieldCount);
    int isdeleted, presetdeleted;
    presetdeleted = 0;

    const char *timestamp_format = "Y-m-d H:i:s";
    const char *time_format = "H:i:s";
    const char *date_format = "Y-m-d";

    char *recordData = NULL;
    /* Allocate memory for record */
    if((recordData = (char *) g_pxdoc->malloc(g_pxdoc, recordsize, "Allocate memory for record.")) == NULL) {
        PX_close(g_pxdoc);
        return;
    }

    g_pxf = PX_get_fields(g_pxdoc);
    QStringList horizontalHeaderLabels;
    for(int nField = 0; nField < nFieldCount; nField++) {
        horizontalHeaderLabels << g_pxf->px_fname;
        g_pxf++;
    }
    m_pTableModel->setHorizontalHeaderLabels(horizontalHeaderLabels);


    /* Output records */
    for(int nRecord = 0; nRecord < nRecordCount; nRecord++) {
        int nOffset = 0;
        pxdatablockinfo_t pxdbinfo;
        isdeleted = presetdeleted;

        int isdeleted=0;
        if(PX_get_record2(g_pxdoc, nRecord, recordData, &isdeleted, &pxdbinfo)) {
            g_pxf = PX_get_fields(g_pxdoc);

            for(int nField = 0; nField < nFieldCount; nField++) {
                QStandardItem *pItem = NULL;
                switch(g_pxf->px_ftype) {
                    case pxfAlpha: {
                        char *value;
                        int ret;
                        if(0 < (ret = PX_get_data_alpha(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value))) {
                            pItem = new QStandardItem(QString::fromUtf8(value));
                            pItem->setData(g_pxf->px_ftype, Qt::UserRole);
                            m_pTableModel->setItem(nRecord, nField, pItem);
                        } else if(ret < 0) {
                            QMessageBox::information(this, QString("Warnning"), QString("Error while reading data of field number %1").arg(nField + 1));
                        }
                        break;
                    }
                    case pxfDate: {
                        long value;
                        if(0 < PX_get_data_long(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            char *str = PX_timestamp2string(g_pxdoc, (double) value*1000.0 * 86400.0, date_format);
                            pItem = new QStandardItem(str);
                            m_pTableModel->setItem(nRecord, nField, pItem);
                            g_pxdoc->free(g_pxdoc, str);
                        }
                        break;
                    }
                    case pxfShort: {
                        short int value;
                        if(0 < PX_get_data_short(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            pItem = new QStandardItem(QString::number(value));
                            m_pTableModel->setItem(nRecord, nField, pItem);
                        }
                        break;
                    }
                    case pxfAutoInc:
                    case pxfLong: {
                        long value;
                        if(0 < PX_get_data_long(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            pItem = new QStandardItem(QString::number(value));
                            m_pTableModel->setItem(nRecord, nField, pItem);
                        }
                        break;
                    }
                    case pxfTimestamp: {
                        double value;
                        if(0 < PX_get_data_double(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            char *str = PX_timestamp2string(g_pxdoc, value, timestamp_format);
                            pItem = new QStandardItem(str);
                            m_pTableModel->setItem(nRecord, nField, pItem);
                            g_pxdoc->free(g_pxdoc, str);
                        }
                        break;
                    }
                    case pxfTime: {
                        long value;
                        if(0 < PX_get_data_long(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            char *str = PX_timestamp2string(g_pxdoc, (double) value, time_format);
                            pItem = new QStandardItem(str);
                            m_pTableModel->setItem(nRecord, nField, pItem);
                            g_pxdoc->free(g_pxdoc, str);
                        }
                        break;
                    }
                    case pxfCurrency:
                    case pxfNumber: {
                        double value;
                        if(0 < PX_get_data_double(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            pItem = new QStandardItem(QString::number(value));
                            m_pTableModel->setItem(nRecord, nField, pItem);
                        }
                        break;
                    }
                    case pxfLogical: {
                        char value;
                        if(0 < PX_get_data_byte(g_pxdoc, &recordData[nOffset], g_pxf->px_flen, &value)) {
                            if(value) {
                                pItem = new QStandardItem(QString::number(1));
                                m_pTableModel->setItem(nRecord, nField, pItem);
                            }
                            else {
                                pItem = new QStandardItem(QString::number(0));
                                m_pTableModel->setItem(nRecord, nField, pItem);
                            }
                        }
                        break;
                    }
                    case pxfGraphic:
                    case pxfBLOb:
                    case pxfFmtMemoBLOb:
                    case pxfMemoBLOb:
                    case pxfOLE:
                        break;
                    }
                    case pxfBytes: {
                        break;
                    }
                    case pxfBCD: {
                        break;
                    }
                    default:
                        break;
                }

                if(pItem) {
                    pItem->setData(g_pxf->px_ftype, Qt::UserRole);
                }


                nOffset += g_pxf->px_flen;
                g_pxf++;
            }
        }
    }

    g_pxdoc->free(g_pxdoc, recordData);
}

void MainWindow::on_actionSave_Database_triggered()
{

}

void MainWindow::on_pbAdd_clicked()
{
    if(g_pxdoc == NULL)
        return;

    int nColCount = m_pTableModel->columnCount();
    QList<QStandardItem*> newRow;
    for(int i = 0; i < nColCount; i++)
        newRow << new QStandardItem("");

    m_pTableModel->appendRow(newRow);

    pxval_t **dataptr;
    /* Allocate memory for return record */
    if(NULL == (dataptr = (pxval_t **) g_pxdoc->malloc(g_pxdoc, g_pxh->px_numfields * sizeof(pxval_t *), "Allocate memory for array of pointers to field values."))) {
        QMessageBox::information(this, QString("Warnning"), QString("Could not allocate memory for array of pointers to field values"));
        return;
    }

    g_pxf = PX_get_fields(g_pxdoc);
    for(int i = 0; i < m_pTableModel->columnCount(); i++) {
        MAKE_PXVAL(g_pxdoc, dataptr[i]);
        dataptr[i]->type = g_pxf->px_ftype;
        g_pxf++;
    }

    PX_insert_record(g_pxdoc, dataptr);
//    PX_write_primary_index(g_pxdoc, g_pindexdoc);
}

void MainWindow::on_pbRemove_clicked()
{
    if(g_pxdoc == NULL || m_pTableModel->rowCount() == 0)
        return;

    // Select a row programmatically (you can also select rows interactively in the UI)
    QItemSelectionModel *selectionModel = ui->tvTableContent->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    if (!selectedIndexes.isEmpty()) {
        // Assuming a single selection, get the row index
        int nRow = selectedIndexes.first().row();
        m_pTableModel->removeRow(nRow);
        PX_delete_record(g_pxdoc, nRow);
//        PX_delete_record(g_pindexdoc, nRow);
//        PX_write_primary_index(g_pxdoc, g_pindexdoc);
    }
}

void MainWindow::on_pbDiff_clicked()
{

}
