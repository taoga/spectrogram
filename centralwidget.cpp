#include "centralwidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QTime>



CCentralWidget::CCentralWidget(QWidget *parent) :
    QWidget(parent)
{
    m_u64Calc = 0;
    m_u64Update = 0;
    m_pChartPrm = 0;
    m_nTimer = -1;

    m_bRead4File = false;

    m_pChartPrm = new CChartPrm;
    m_pSpgmWgt = 0;
    m_pSpgmWgt = new CSpgramm();

    QVBoxLayout* pvbxLayout = new QVBoxLayout(this);

    if( m_pSpgmWgt && m_pChartPrm && pvbxLayout )
    {
        m_pSpgmWgt->setMinimumHeight(200);
        onUpdatePrms( 0 );
        //Layout setup
        pvbxLayout->addWidget( m_pChartPrm, 1 );
        pvbxLayout->addWidget( m_pSpgmWgt, 5 );

        setLayout( pvbxLayout );

        connect( m_pChartPrm, SIGNAL(sigUpdate(quint8)), this, SLOT(onUpdatePrms(quint8)));
        connect( m_pChartPrm, SIGNAL(sigStart(quint8)), this, SLOT(onStart(quint8)));
        connect( m_pChartPrm, SIGNAL(Scale()), this, SLOT(onScale()));
        connect( m_pChartPrm, SIGNAL(Mrk1(bool)), this, SLOT(onMrk1(bool)));
        connect( m_pChartPrm, SIGNAL(Mrk2(bool)), this, SLOT(onMrk2(bool)));

        connect( m_pSpgmWgt, SIGNAL(sigUpdatedMrk1(double,double)), m_pChartPrm, SLOT(onUpdatedMrk1(double,double)) );
        connect( m_pSpgmWgt, SIGNAL(sigUpdatedMrk2(double,double)), m_pChartPrm, SLOT(onUpdatedMrk2(double,double)) );
    }
}

CCentralWidget::~CCentralWidget()
{
    if( m_nTimer != -1) killTimer( m_nTimer );

    if( m_pSpgmWgt ) delete m_pSpgmWgt;
    if( m_pChartPrm ) delete m_pChartPrm;
}

void CCentralWidget::onStart( quint8 ui8Mode )
{
    if( ui8Mode )
    {
        onUpdatePrms( 0 );
        // Start
        if( m_pSpgmWgt )
        {
            m_pSpgmWgt->ResetMrks();
            m_pSpgmWgt->FullUpdate();
            m_pSpgmWgt->ResizeSpgm();  // Обновить размер
        }
        if( m_pChartPrm )
            m_pChartPrm->SetMrkChecked( false );

        m_u64Calc = 0;
        m_u64Update = 0;

        m_nTimer = startTimer( 10 );
    }
    else
    {
        // Stop
        if( m_nTimer != -1) killTimer( m_nTimer );
        m_nTimer = -1;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// Обновление параметров //////////////////////////////////////////////////////////////
// quint8 ui8Mode - параметры обновления
// 0 - полное обновление
// 1 - увеличение масштаба
// 2 - уменьшение масштаба
void CCentralWidget::onUpdatePrms( quint8 ui8Mode )
{
    if( !m_pChartPrm || !m_pSpgmWgt ) return;

    if( ui8Mode == 0 )
    {
        //
        m_pSpgmWgt->SetTitle( m_pChartPrm->GetTitleSpgm() );
        m_pSpgmWgt->SetDrawTitle( m_pChartPrm->GetDrawTitle() );
        //
        m_pSpgmWgt->SetFFTSize( m_pChartPrm->GetFftSize() );
        // Calculate scale for axe X
        double lfMin = m_pChartPrm->GetMinX(), lfMax = m_pChartPrm->GetMaxX();
        double lfScale = (lfMax - lfMin) / (double)(m_pSpgmWgt->GetFFTSize() - 1);

        m_pSpgmWgt->SetAxe( CSpgramm::X, m_pChartPrm->GetTitleX(), m_pChartPrm->GetUnitX(), lfMin, lfMax, lfScale, 0, 0, LINEAR_FACTOR_SCALE, m_pChartPrm->GetPrecX() );

        lfScale = (double)(m_pSpgmWgt->GetFFTSize() - 1) / (lfMax - lfMin);
        lfMin = lfMax = 0.0;
        m_pSpgmWgt->SetAxe( CSpgramm::Y, m_pChartPrm->GetTitleY(), m_pChartPrm->GetUnitY(), lfMin, lfMax, lfScale, 0, 0, LINEAR_FACTOR_SCALE, m_pChartPrm->GetPrecY() );
        m_pSpgmWgt->SetAxe( CSpgramm::Z, m_pChartPrm->GetTitleZ(), m_pChartPrm->GetUnitZ(), m_pChartPrm->GetMinZ(), m_pChartPrm->GetMaxZ(), 1.0f, 0, 0, 1, m_pChartPrm->GetPrecZ());
        // Сдвиг спектрограммы( снизу вверх или сверху вниз)
        m_pSpgmWgt->SetShiftUp2Down( m_pChartPrm->GetShiftUp2Down() );
        m_pSpgmWgt->FullUpdate();
    }
    else
    if( ui8Mode == 1 )
    {
    }
    else
    if( ui8Mode == 2 )
    {
    }
}

void CCentralWidget::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    if( m_pSpgmWgt )
    {
        quint32 u32FFTSize = m_pSpgmWgt->GetFFTSize(), u32Pos = 0;
        qint8   i8Rand, i8Min = 127, i8Max = -128;
        float *pfBuffer = m_pSpgmWgt->GetFFTBuffer();

        for( u32Pos = 0; u32Pos < u32FFTSize; u32Pos++ )
        {
            i8Rand = rand() & 0x7F;
            if( i8Min > i8Rand ) i8Min = i8Rand;
            if( i8Max < i8Rand ) i8Max = i8Rand;
            i8Rand = i8Rand - 120;
            pfBuffer[u32Pos] = (float)i8Rand;
        }
        m_pSpgmWgt->AddFFTBuffer();
    }
}

void CCentralWidget::onScale()
{
    if( m_pSpgmWgt ) {
        m_pSpgmWgt->DecScale();
    }
}

void CCentralWidget::onMrk1(bool bChecked)
{
    if( bChecked ){
        if( m_pSpgmWgt ) m_pSpgmWgt->OnMrk1( true );
    }
    else{
        if( m_pSpgmWgt ) m_pSpgmWgt->OnMrk1( false );
    }
}

void CCentralWidget::onMrk2( bool bChecked )
{
    if( bChecked ){
        if( m_pSpgmWgt ) m_pSpgmWgt->OnMrk2( true );
    }
    else{
        if( m_pSpgmWgt ) m_pSpgmWgt->OnMrk2( false );
    }
}
