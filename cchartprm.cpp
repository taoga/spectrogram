#include "cchartprm.h"
#include "ui_cchartprm.h"

CChartPrm::CChartPrm(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CChartPrm)
{
    ui->setupUi(this);

    m_u8Start = 0;
    // Set default values
    // X
    ui->leTitleX->setText("Frequency");
    ui->leUnitX->setText("Hz");
    ui->leMinX->setText("100000000");
    ui->leMaxX->setText("102500000");
    ui->lePrecX->setText("2");
    // Y
    ui->leTitleY->setText("Time");
    ui->leUnitY->setText("sec.");
    ui->leMinY->setText("0");
    ui->leMaxY->setText("0");
    ui->lePrecY->setText("2");
    // Z
    ui->leTitleZ->setText("Power");
    ui->leUnitZ->setText("dBm");
    ui->leMinZ->setText("-120");
    ui->leMaxZ->setText("5");
    ui->lePrecZ->setText("2");
    // Init FFT
    quint64         u64FFTPoints = 64;
    QStringList     qslFfts;
    QString         qsTemp;
    qint8           nCur = -1;

    for( quint8 u8I = 0; u8I < 20; u8I++ )
    {
        qsTemp = QString::asprintf( "%llu",  u64FFTPoints );
        qslFfts.push_back( qsTemp );
        if( u64FFTPoints == 16384 ) nCur = u8I;
        u64FFTPoints *= 2;
    }

    ui->cbFftSize->addItems( qslFfts );
    if( nCur != -1 )
        ui->cbFftSize->setCurrentIndex( nCur );

    ui->leTitle->setText("Spectrogramm. Span:");

    ui->tbtnMrk1->setCheckable( true );
    ui->tbtnMrk1->setChecked( false );
    ui->tbtnMrk2->setCheckable( true );
    ui->tbtnMrk2->setChecked( false );

    m_lfMrk1X = m_lfMrk1Y = 0.0;
    m_lfMrk2X = m_lfMrk2Y = 0.0;
}

CChartPrm::~CChartPrm()
{
    delete ui;
}

void CChartPrm::on_pbUpdate_clicked()
{
    // Послать сигнал родителю, что требуется обновить данные
    emit sigUpdate( 0 );
}

quint64 CChartPrm::GetFftSize()
{
    if( ui->cbFftSize->currentIndex() != -1 )
    {
        QString qsTemp = ui->cbFftSize->itemText( ui->cbFftSize->currentIndex() );
        if( qsTemp.length() > 0 )
            return qsTemp.toULongLong();
        else
            return DEF_FFT_SIZE;

    }
    else
        return DEF_FFT_SIZE;
}

bool CChartPrm::GetDrawTitle()
{
    return ui->checkDrawTitle->isChecked();
}

bool CChartPrm::GetShiftUp2Down()
{
    return ui->checkShiftUp2Down->isChecked();
}

QString CChartPrm::GetTitleSpgm()
{
    return ui->leTitle->text();
}

// X
QString CChartPrm::GetTitleX()
{
    return ui->leTitleX->text();
}

QString CChartPrm::GetUnitX()
{
    return ui->leUnitX->text();
}

double CChartPrm::GetMinX()
{
    return ui->leMinX->text().toDouble();
}

double CChartPrm::GetMaxX()
{
    return ui->leMaxX->text().toDouble();
}

quint16 CChartPrm::GetPrecX()
{
    return ui->lePrecX->text().toUInt();
}
// Y
QString CChartPrm::GetTitleY()
{
    return ui->leTitleY->text();
}

QString CChartPrm::GetUnitY()
{
    return ui->leUnitY->text();
}

double CChartPrm::GetMinY()
{
    return ui->leMinY->text().toDouble();
}

double CChartPrm::GetMaxY()
{
    return ui->leMaxY->text().toDouble();
}

quint16 CChartPrm::GetPrecY()
{
    return ui->lePrecY->text().toUInt();
}
// Z
QString CChartPrm::GetTitleZ()
{
    return ui->leTitleZ->text();
}

QString CChartPrm::GetUnitZ()
{
    return ui->leUnitZ->text();
}

double CChartPrm::GetMinZ()
{
    return ui->leMinZ->text().toDouble();
}

double CChartPrm::GetMaxZ()
{
    return ui->leMaxZ->text().toDouble();
}

quint16 CChartPrm::GetPrecZ()
{
    return ui->lePrecZ->text().toUInt();
}

void CChartPrm::on_pbStart_clicked()
{
    m_u8Start ^= 1;

    if( !m_u8Start )
    {
        ui->pbStart->setText("Start");
        ui->cbFftSize->setEnabled( true );
    }
    else
    {
        ui->pbStart->setText("Stop");
        ui->cbFftSize->setEnabled( false );
    }

    emit sigStart( m_u8Start );
}

// Уменьшить масштаб
void CChartPrm::on_tbtnScale_clicked()
{
    emit Scale();
}
// Показать маркер1
void CChartPrm::on_tbtnMrk1_clicked()
{
    emit Mrk1( ui->tbtnMrk1->isChecked() );
}
// Показать маркер2
void CChartPrm::on_tbtnMrk2_clicked()
{
    emit Mrk2( ui->tbtnMrk2->isChecked() );
}

void CChartPrm::SetMrkChecked( bool bCheck )
{
    ui->tbtnMrk1->setChecked( bCheck );
    ui->tbtnMrk2->setChecked( bCheck );
}

void CChartPrm::onUpdatedMrk1( double lfX, double lfY )
{
    //qDebug() << "CChartPrm::onUpdatedMrk1 (" << lfX << " ," << lfY << ")";
    QString qsTemp;

    m_lfMrk1X = lfX;
    m_lfMrk1Y = lfY;

    if( m_lfMrk1X == 0.0 )
    {
        qsTemp.clear();

        ui->leX1->setText( qsTemp );
        ui->leY1->setText( qsTemp );
    }
    else
    {
        qsTemp = QString::asprintf("%.0lf", lfX );

        ui->leX1->setText( qsTemp );

        qsTemp = QString::asprintf("%lf", lfY );

        ui->leY1->setText( qsTemp );
    }
}

void CChartPrm::onUpdatedMrk2( double lfX, double lfY )
{
    //qDebug() << "CChartPrm::onUpdatedMrk2 (" << lfX << " ," << lfY << ")";
    QString qsTemp;

    m_lfMrk2X = lfX;
    m_lfMrk2Y = lfY;

    if( m_lfMrk2X == 0.0 )
    {
        qsTemp.clear();

        ui->leX2->setText( qsTemp );
        ui->leY2->setText( qsTemp );
        ui->leDX->setText( qsTemp );
        ui->leDY->setText( qsTemp );
    }
    else
    {
        qsTemp = QString::asprintf("%.0lf", lfX );

        ui->leX2->setText( qsTemp );

        qsTemp = QString::asprintf("%lf", lfY );

        ui->leY2->setText( qsTemp );

        // Вычислить разницу
        double lfDX = m_lfMrk2X - m_lfMrk1X;
        double lfDY = m_lfMrk2Y - m_lfMrk1Y;

        qsTemp = QString::asprintf("%.0lf", lfDX );
        ui->leDX->setText( qsTemp );

        qsTemp = QString::asprintf("%lf", lfDY );
        ui->leDY->setText( qsTemp );
    }
}
