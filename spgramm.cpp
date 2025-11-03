#include "spgramm.h"
#include <QDebug>
#include <QPainter>
#include <QApplication>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#else
#endif
#include <QMouseEvent>
#include "math.h"

CSpgramm::CSpgramm(QWidget *parent) : QWidget(parent)
{
    m_bDrawTitle = false;
    m_bShiftUp2Down = false;
    m_u8CurRGB = 0;

    // Statistics
    m_u64In = 0;
    m_u64Out = 0;
    m_u64InProc = 0;
    m_u64InProcMax = 0;

    m_ptPosMrk1 = QPoint( -1, -1 );
    m_ptPosMrk2 = QPoint( -1, -1 );
    m_ptPosMrk = QPoint( -1, -1 );
    m_ptPosScale1 = QPoint( -1, -1 );
    m_ptPosScale2 = QPoint( -1, -1 );

    m_bMrk1 = false;
    m_bMrk2 = false;

    m_u16TimePerSecUpd = 1;
    m_u16SpgmPerSecUpd = 1;
    m_bUpdateY = false;
    m_bUpdateX = false;
    m_bFullUpdate = false;
    m_bPauseUpdate = false;
    m_i16Top = 0;
    m_i16Left = 0;
    m_i16Bottom = 0;
    m_i16Right = 0;
    // Temporarily
    m_i16Top = m_i16Left = m_i16Bottom = m_i16Right = DEFAULT_ORIGIN;
    m_u32FFTSize = 0;
    m_u16CurRd = 0;
    m_u16CurWr = 0;
    m_lfColorScale = 1.0;
    // The upper and lower limits of using the color table as a percentage
    m_lfPercUseColorsDown = 0.0;
    m_lfPercUseColorsUp = 1.0;
    // Array on 3 axes
    m_AxeDesc.resize( 3 );

    m_bkColor = QColor( 0, 0, 0, 255 );             // Цвет Background color
    m_textColor = QColor( 255, 255, 255, 255 );		// Text color
    m_lineColor = QColor( 200, 200, 200, 255 );     // Line color

    m_pLinePen = 0;
    m_pTextPen = 0;
    m_pMrk1Pen = 0;
    m_pMrk2Pen = 0;

    m_pTopImage = 0;
    m_pBotImage = 0;
    m_pLeftImage = 0;
    m_pRightImage = 0;

    m_u32OrgY = 0;

    m_pSemUpdate = 0;
    m_pSemUpdate = new QSemaphore(0);

    m_vecScale.resize( 1 );
    m_u16CurScale = 0;

    m_defFont = QFont("Helvetica [Cronyx]", 11 );

    m_qthreadUpdate.SetParent( this );

    connect( &m_qthreadUpdate, SIGNAL(sigUpdate()), this, SLOT( onUpdate() ), Qt::QueuedConnection  );

    m_qthreadUpdate.start();
    setMouseTracking( true ); // Track the movement of the mouse cursor
}

CSpgramm::~CSpgramm()
{
    m_qthreadUpdate.StopThread();
    QThread::msleep( 200 );

    ReleaseFFTBuffers();
    ReleasePainterMem();

    if( m_pSemUpdate ) delete m_pSemUpdate;
}

void CSpgramm::ReleasePainterMem()
{
    if ( m_pLinePen )
    {
        delete m_pLinePen;
        m_pLinePen = 0;
    }

    if ( m_pTextPen )
    {
        delete m_pTextPen;
        m_pTextPen = 0;
    }

    if ( m_pMrk1Pen )
    {
        delete m_pMrk1Pen;
        m_pMrk1Pen = 0;
    }

    if ( m_pMrk2Pen )
    {
        delete m_pMrk2Pen;
        m_pMrk2Pen = 0;
    }

    if( m_pTopImage )
    {
        delete m_pTopImage;
        m_pTopImage = 0;
    }

    if( m_pBotImage )
    {
        delete m_pBotImage;
        m_pBotImage = 0;
    }

    if( m_pLeftImage )
    {
        delete m_pLeftImage;
        m_pLeftImage = 0;
    }

    if( m_pRightImage )
    {
        delete m_pRightImage;
        m_pRightImage = 0;
    }
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::paintEvent : Handler for the element rendering event
/// \param event
void CSpgramm::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter    painter(this);

    LockImageRGB();
    painter.drawImage( m_i16Left, m_i16Top, ( !m_u8CurRGB ) ? m_imageRGB_0 : m_imageRGB_1 );
    UnlockImageRGB();

    if( m_bUpdateY )
        DrawLeftYText();

    if( m_bUpdateX )
    {
        if( m_bDrawTitle)
            PrepareImageTopX( false );
        PrepareImageBotX( false );
    }

    // The X-axis is on top
    if( m_pTopImage && m_bDrawTitle )
        painter.drawImage( 0, 0, *m_pTopImage );
    // The X-axis is from the bottom
    if( m_pBotImage )
        painter.drawImage( 0, m_i16Top + m_rectSpgm.height(), *m_pBotImage );
    // The Y axis is on the left
    if( m_pLeftImage )
        painter.drawImage( 0, m_i16Top, *m_pLeftImage );
    // The Y-axis is on the right
    if( m_pRightImage )
        painter.drawImage( m_qsizeClient.width() -  m_i16Right, m_i16Top, *m_pRightImage );

    // The Spectrogram line from above
    if( !m_bDrawTitle )
    {
        painter.setPen( *m_pLinePen );
        painter.drawLine( m_i16Left, 0, m_i16Left + m_rectSpgm.width(), 0 );
    }

    if( m_bPauseUpdate )    // The spectrogram is not updated
    {
        QPainter::CompositionMode curCompMode = painter.compositionMode();
        painter.setCompositionMode(QPainter::RasterOp_SourceXorDestination); // xor op

        if( m_ptPosMrk1.x() != -1 )
        {
            painter.setPen( *m_pMrk1Pen );
            painter.drawLine( m_ptPosMrk1.x(), m_rectSpgm.top(), m_ptPosMrk1.x(), m_rectSpgm.bottom());
            painter.drawLine( m_rectSpgm.left(), m_ptPosMrk1.y(), m_rectSpgm.right(), m_ptPosMrk1.y() );
        }
        if( m_ptPosMrk2.x() != -1 )
        {
            painter.setPen( *m_pMrk2Pen );
            painter.drawLine( m_ptPosMrk2.x(), m_rectSpgm.top(), m_ptPosMrk2.x(), m_rectSpgm.bottom());
            painter.drawLine( m_rectSpgm.left(), m_ptPosMrk2.y(), m_rectSpgm.right(), m_ptPosMrk2.y() );
        }

        if( m_ptPosScale1.x() != -1 && m_ptPosScale2.x() != -1 )
        {
            painter.setPen( *m_pMrk1Pen );
            painter.drawRect( QRect( m_ptPosScale1, m_ptPosScale2 ) );
        }

        painter.setCompositionMode( curCompMode );
    }
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::PrepareImageLeftY : Подготовить изображение оси Y слева
void CSpgramm::PrepareImageLeftY()
{
    if( m_pLeftImage )
    {
        delete m_pLeftImage;
        m_pLeftImage = 0;
    }
    m_pLeftImage = new QImage( m_i16Left, m_rectSpgm.height() + 1, QImage::Format_RGB32 );
    if( m_pLeftImage )
    {
        QPainter qPainter( m_pLeftImage );
        // Background
        qPainter.fillRect( 0, 0, m_pLeftImage->width(), m_pLeftImage->height(), m_bkColor );
        // Axis Line
        qPainter.setPen( *m_pLinePen );
        qPainter.drawLine( m_i16Left - 1, 0, m_i16Left - 1, m_rectSpgm.height() );
        // Remove the gaps
        DrawScaleDivY( &qPainter, m_i16Left - 2, -DIV_SIZE );
    }
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::DrawScaleDivY  : Draw a scale for the Y axis
/// \param pPainter     - The context of drawing
/// \param i16Shift     - Left margin
/// \param i16DivSize   - Line size("+" - draw to the right; "-" - draw to the left)
void CSpgramm::DrawScaleDivY( QPainter *pPainter, qint16 i16Shift, qint16 i16DivSize )
{
    // Remove the gaps
    int     i, nCount = 0, iSpHeight = m_rectSpgm.height();
    QPoint	pntBottomF, pntBottomL;     // The starting and ending point of the cutting line

    pntBottomL.setX( i16Shift );

    pntBottomF.setY(1);
    pntBottomL.setY(1);

    pPainter->setPen( *m_pLinePen );

    for (i = 0; i < iSpHeight; i += SCALE_STEP)
    {
        if ( !(nCount % 10) )
        {
            // Size of the cut-off line
            pntBottomF.setX( pntBottomL.x() + i16DivSize );
        }
        else
        if ( !(nCount % 5) )
        {
            pntBottomF.setX( pntBottomL.x() + i16DivSize/2 );
        }
        else
        {
            pntBottomF.setX( pntBottomL.x() + i16DivSize/3 );
        }

        pPainter->drawLine( pntBottomF, pntBottomL );

        pntBottomF.ry() += SCALE_STEP;

        pntBottomL.setY( pntBottomF.y() );

        nCount++;
    }
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::DrawLeftYText - Update Y-axis signatures on the left
void CSpgramm::DrawLeftYText()
{
    if( m_u16CurScale >= m_vecScale.size() ) return;
    if( m_AxeDesc.size() <= X || m_AxeDesc.size() <= Y ) return;

    if( m_pLeftImage )
    {
        QPainter qPainter( m_pLeftImage );
        // Background
        qPainter.fillRect( 0, 0, m_i16Left - DIV_SIZE, m_pLeftImage->height(), m_bkColor );
        // Output text
        int     i, nCount = 0, iSpHeight = m_rectSpgm.height(), nNextPos = 0;
        double	lfValueScale = 1.0f, lfValue;	// Scale to calculate the value corresponding to the image unit.
        quint16 u16StartY = m_vecScale[m_u16CurScale].nStartY;
        double  lfScaleY = (double)(m_vecScale[m_u16CurScale].nEndY - m_vecScale[m_u16CurScale].nStartY) / (double)(iSpHeight - 1);
        int     nPosY = 0;

        // Output the gaps and values of the Y axis /////////////////////////////////////////////
        switch ( m_AxeDesc[Y].dType )
        {
        case LINEAR_FACTOR_SCALE:
            lfValueScale = m_AxeDesc[Y].lfStep;
            break;
        default: lfValueScale = (m_AxeDesc[Y].lfMax - m_AxeDesc[Y].lfMin) / (iSpHeight - 1);	// Scale for units of images
        }

        QString		qsFormat, qsTemp;
        // Calculate the row size to output the axis value
        // -999.99K
        qsFormat = QString::asprintf("%%.%dlf", m_AxeDesc[X].dPrecision );

        if( m_AxeDesc[X].dPrecision != 0 )
            qsTemp = QString::asprintf( qsFormat.toStdString().data(), 999.999);
        else
            qsTemp = QString::asprintf( qsFormat.toStdString().data(), 999999.999);

        qsTemp += "K";


        QFontMetrics tmpFontMetric( m_defFont );

        int         iTextX = 0, iTextY = tmpFontMetric.height();
        iTextX = tmpFontMetric.horizontalAdvance(qsTemp);
        quint32     u32OrgY = GetOrgY();

        qPainter.setPen( *m_pTextPen );

        for( i = 0; i < iSpHeight; i += SCALE_STEP )
        {
            if ( !(nCount % 10) )
            {
                if ( nNextPos <= i )
                {
                    nNextPos = i + iTextX;

                    nPosY = (double)i*lfScaleY + u16StartY; // Position based on scale

                    lfValue = 0;
                    if( !m_bShiftUp2Down )
                    {
                        lfValue = (double)((double)nPosY + (double)u32OrgY) * lfValueScale + m_AxeDesc[Y].lfMin;
                    }
                    else
                    {
                        double lfPos = (double)u32OrgY - (double)nPosY;
                        if( lfPos >= 0 )
                            lfValue = (double)(lfPos) * lfValueScale + m_AxeDesc[Y].lfMin;
                    }

                    qsFormat = QString::asprintf( "%%.%dlf", m_AxeDesc[Y].dPrecision );
                    qsTemp = QString::asprintf( qsFormat.toStdString().data(), lfValue );
                    qsTemp += " ";
                    qsTemp += m_AxeDesc[Y].qsUnit;

                    qPainter.save();
                    if( !m_bShiftUp2Down )
                    {
                        qPainter.translate(m_i16Left - DIV_SIZE - (iTextY - iTextY/3), i + 1);
                        qPainter.rotate(90);
                    }
                    else
                    {
                        qPainter.translate(m_i16Left - DIV_SIZE - (iTextY - iTextY/3), i + 1);
                        qPainter.rotate(90);
                    }

                    qPainter.drawText(0,0, qsTemp);
                    qPainter.restore();
                }
            }

            nCount++;
        }
    }
    SetUpdateY( false );
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::PrepareImageRightY - Prepare the Y-axis image on the right
void CSpgramm::PrepareImageRightY()
{
    if( m_pRightImage )
    {
        delete m_pRightImage;
        m_pRightImage = 0;
    }
    m_pRightImage = new QImage( m_i16Right, m_rectSpgm.height() + 1, QImage::Format_RGB32 );
    if( m_pRightImage )
    {
        QPainter qPainter( m_pRightImage );
        // Background
        qPainter.fillRect( 0, 0, m_pRightImage->width(), m_pRightImage->height(), m_bkColor );
        // Axis Line
        qPainter.setPen( *m_pLinePen );
        qPainter.drawLine( 0, 0, 0, m_rectSpgm.height() );
        // Remove the gaps
        DrawScaleDivY( &qPainter, 1, DIV_SIZE );
    }
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::PrepareImageTopX - Prepare an image of the X-axis from above
/// \param bReleaseMem
void CSpgramm::PrepareImageTopX(bool bReleaseMem)
{
    if( m_u16CurScale >= m_vecScale.size() ) return;
    if( m_AxeDesc.size() <= X || m_AxeDesc.size() <= Y ) return;

    if( bReleaseMem )
    {
        if( m_pTopImage )
        {
            delete m_pTopImage;
            m_pTopImage = 0;
        }
        m_pTopImage = new QImage( m_qsizeClient.width(), m_i16Top, QImage::Format_RGB32 );
    }
    if( m_pTopImage )
    {
        QPainter qPainter( m_pTopImage );
        QString  qsTemp, qsFormat, qsTitle;
        // Background
        qPainter.fillRect( 0, 0, m_pTopImage->width(), m_pTopImage->height(), m_bkColor );
        // Axis Line
        qPainter.setPen( *m_pLinePen );
        qPainter.drawLine( m_i16Left - 1, m_i16Top - 1, m_i16Left + m_rectSpgm.width(), m_i16Top - 1 );

        QRect   layoutRect;
        layoutRect = QRect( 0, 0, m_qsizeClient.width(), m_i16Top - 1 );
        qPainter.setPen( *m_pTextPen );

        double lfBand = m_AxeDesc[X].lfMax - m_AxeDesc[X].lfMin;
        qsFormat = QString::asprintf( "%%.%dlf", m_AxeDesc[X].dPrecision );
        if( m_AxeDesc[X].dPrecision != 0 )
            ReduceValue( qsFormat, lfBand );
        qsTemp = QString::asprintf( qsFormat.toStdString().data(), lfBand );

        qsTitle += m_qsTitle;
        qsTitle += qsTemp;
        qsTitle += m_AxeDesc[X].qsUnit;

        qPainter.drawText( layoutRect, Qt::AlignHCenter | Qt::AlignVCenter, qsTitle );
    }
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::PrepareImageBotX - Prepare an image of the X-axis from below
/// \param bReleaseMem
void CSpgramm::PrepareImageBotX(bool bReleaseMem)
{
    if( m_u16CurScale >= m_vecScale.size() ) return;
    if( m_AxeDesc.size() <= X || m_AxeDesc.size() <= Y ) return;

    if( bReleaseMem )
    {
        if( m_pBotImage )
        {
            delete m_pBotImage;
            m_pBotImage = 0;
        }
        m_pBotImage = new QImage( m_qsizeClient.width(), m_i16Bottom, QImage::Format_RGB32 );
    }
    if( m_pBotImage )
    {
        QPainter qPainter( m_pBotImage );
        // Background
        qPainter.fillRect( 0, 0, m_pBotImage->width(), m_pBotImage->height(), m_bkColor );
        //Axis Line
        qPainter.setPen( *m_pLinePen );
        qPainter.drawLine( m_i16Left - 1, 0, m_i16Left + m_rectSpgm.width(), 0 );
        // Notches
        // Inscription
        QString		qsFormat, qsTemp;
        // Calculate the row size to output the axis value
        // -999.99K
        qsFormat = QString::asprintf("%%.%dlf", m_AxeDesc[X].dPrecision );

        if( m_AxeDesc[X].dPrecision != 0 )
            qsTemp = QString::asprintf( qsFormat.toStdString().data(), -999.999);
        else
            qsTemp = QString::asprintf( qsFormat.toStdString().data(), 999999.999);

        qsTemp += "K";


        QFontMetrics tmpFontMetric( m_defFont );

        int iTextX = 0, iTextY = tmpFontMetric.height();
        iTextX = tmpFontMetric.horizontalAdvance(qsTemp);
        // Output the gaps and values of the X-axis /////////////////////////////////////////////
        double	lfValue;
        double	lfValueScale;	// Scale for units of images
        int		i = 0, nCount = 0, nNextPos = 0, iSpWidth = m_rectSpgm.width();
        QRect   layoutRect;
        QPoint	pntBottomF, pntBottomL;     // The starting and ending point of the cutting line
        quint16 u16StartX = m_vecScale[m_u16CurScale].nStartX;
        double  lfScaleX = (double)(m_vecScale[m_u16CurScale].nEndX - m_vecScale[m_u16CurScale].nStartX) / (double)(iSpWidth - 1);
        quint16 nPosFFT = 0;

        switch ( m_AxeDesc[X].dType )
        {
            case LINEAR_FACTOR_SCALE:	lfValueScale = m_AxeDesc[X].lfStep;
                break;
            default:
                lfValueScale = (m_AxeDesc[X].lfMax - m_AxeDesc[X].lfMin) / (m_u32FFTSize - 1);
        }

        nCount = 0;		// The cutoff counter

        pntBottomF.setX(m_i16Left);
        pntBottomL.setX(m_i16Left);
        pntBottomF.setY(1);

        for (i = 0; i < iSpWidth; i += SCALE_STEP)
        {
            if ( !(nCount % 10) )
            {
                // Size of the cut-off line
                pntBottomL.setY( pntBottomF.y() + DIV_SIZE - 1 );

                // Print the signature text
                if ( nNextPos <= i )
                {
                    nNextPos = i + iTextX;
                    nPosFFT = (quint16)((double)(i * lfScaleX)) + u16StartX;
                    // Calculate, collapse value, output text
                    switch ( m_AxeDesc[X].dType )
                    {
                        case LINEAR_FACTOR_SCALE:// The scale factor was calculated earlier
                        case LINEAR_SCALE:
                            // pos.on the screen -> single image -> value
                            lfValue = (double)nPosFFT * lfValueScale + m_AxeDesc[X].lfMin;
                            break;			// Linear value
                        //case EXP_SCALE: lfValue = (double)( pow(m_AxeDesc[X].lfStep, floor( (double)i / lfScale ) ) * m_AxeDesc[X].lfMin);			// The extra scale
                        //    break;			//
                    }
                    qsFormat = QString::asprintf( "%%.%dlf", m_AxeDesc[X].dPrecision );
                    if( m_AxeDesc[X].dPrecision != 0 )
                        ReduceValue( qsFormat, lfValue );
                    qsTemp = QString::asprintf( qsFormat.toStdString().data(), lfValue );

                    layoutRect = QRect( m_i16Left + i, iTextY - (iTextY / 3), iTextX, iTextY );
                    qPainter.setPen( *m_pTextPen );
                    qPainter.drawText( layoutRect, Qt::AlignTop | Qt::AlignLeft, qsTemp );
                }
            }
            else
            if ( !(nCount % 5) )
            {
                pntBottomL.setY( pntBottomF.y() + DIV_SIZE/2 );
            }
            else
            {
                pntBottomL.setY( pntBottomF.y() + DIV_SIZE/3 );
            }

            qPainter.setPen( *m_pLinePen );
            qPainter.drawLine( pntBottomF, pntBottomL );

            pntBottomF.rx() += SCALE_STEP;
            pntBottomL.setX( pntBottomF.x() );

            nCount++;
        }

        layoutRect = QRect( m_i16Left + i + 2, 1 , iTextX, iTextY );
        qPainter.setPen( *m_pTextPen );
        qPainter.drawText( layoutRect, Qt::AlignTop | Qt::AlignLeft, m_AxeDesc[X].qsUnit );
    }

    m_bUpdateX = false;
}
/////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::ReduceValue : Shorten the length of the value
/// \param qsFormat - the display format for sprintf
/// \param lfValue  - variable value
void CSpgramm::ReduceValue(QString &qsFormat, double &lfValue)
{
    if ( lfValue > 1000000000.0 ) {lfValue /= 1000000000.0;qsFormat += "Г";}		// Trillions: G
    else
    if ( lfValue > 1000000.0 ) {lfValue /= 1000000.0;qsFormat += "М";}				// Millions: M
    else
    if ( lfValue > 1000.0 ) {lfValue /= 1000.0;qsFormat += "K";}					// Thousands: К
    else
    if ( lfValue > 1.0 ) {qsFormat += "";}											// Units
    else
    if ( lfValue > 0.001 ) {lfValue *= 1000.0;qsFormat += "м";}						// Millie
    else
    if ( lfValue > 0.000001 ) {lfValue *= 1000000.0;qsFormat += "мк";}				// micro
    else
    if ( lfValue > 0.000000001 ) {lfValue *= 1000000000.0;qsFormat += "н";}			// nano
    else
    if ( lfValue > 0.000000000001 ) {lfValue *= 1000000000000.0;qsFormat += "п";}	// Pico
    else
    if ( lfValue > 0.000000000000001 ) {lfValue *= 1000000000000000.0;qsFormat += "ф";}	// Ferra
}
///////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::SetFFTSize : Initialize an array of spectra
/// \param u32FFTSize - Размер спектра
void CSpgramm::SetFFTSize( quint32 u32FFTSize )
{
    if( u32FFTSize > 0 )
    {
        bool bError = false;

        if( u32FFTSize != m_u32FFTSize )
        {
            ReleaseFFTBuffers();

            m_u32FFTSize = u32FFTSize;  // Current FFT size

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            QRect qrectScreenSize = QApplication::desktop()->screenGeometry();
#else
            QRect qrectScreenSize = screen()->geometry();
#endif
            // The size of the internal FFT buffer is equal to the height of the number of points vertically
            quint16 u16FFT = 0, u16FFTCount  = qrectScreenSize.height() * 2;
            for( u16FFT = 0; u16FFT < u16FFTCount; u16FFT++ )
            {
                // a buffer for calculating the FFT for complex numbers (col.counts*2) + 2(requires library)
                float *pScBuf = new float[ (u32FFTSize << 1) + 2];
                if( pScBuf )
                {
                    m_vecFFTs.push_back( pScBuf );
                }
                else
                    bError = true;
            }
        }

        if( bError ) ReleaseFFTBuffers();
        SetCurWr( 0 );
        SetCurRd( 0 );
    }
}
///////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::ReleaseFFTBuffers : Free up the memory of the spectrum array
void CSpgramm::ReleaseFFTBuffers()
{
    quint32 u16FFTCount = m_vecFFTs.size(), u16FFT = 0;

    if( u16FFTCount > 0 )
    {
        for( u16FFT = u16FFTCount - 1; u16FFT > 0; u16FFT-- )
        {
            if( m_vecFFTs[u16FFT] ) delete [] (m_vecFFTs[u16FFT]);
        }
        m_vecFFTs.clear();
    }
    m_u32FFTSize = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::GetFFTBuffer : Return the pointer to the current spectrum buffer
/// \return Pointer to the current spectrum buffer
float* CSpgramm::GetFFTBuffer()
{
    // Block the update
    LockImageRGB();

    quint16 u16FFTCount = (quint16)m_vecFFTs.size();
    float   *pBuffer = 0;

    if( !m_bShiftUp2Down )
    {// The new spectrum is placed at the bottom of the spectrogram
        if( u16FFTCount > 0 && m_u16CurWr < u16FFTCount )
        {
            pBuffer = m_vecFFTs[m_u16CurWr];    // It will remain at the end
            m_u16CurWr++;
        }
    }
    else
    {// Buffer for the new spectrum
        if( u16FFTCount > 0 )
        {
            m_u16CurWr++;
            pBuffer = m_vecFFTs[u16FFTCount - (m_u16CurWr - m_u16CurRd)];   // The last one will be moved to the beginning.
        }
    }
    m_u64InProc++;
    if( m_u64InProc > m_u64InProcMax )
        m_u64InProcMax = m_u64InProc;

    UnlockImageRGB();

    return pBuffer;
}
///////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::AddFFTBuffer : Обновить данные текущего буфера спектра
void CSpgramm::AddFFTBuffer()
{
    if ( Z >= m_AxeDesc.size() ) return;
    if( m_vecFFTs.size() == 0 || m_rectSpgm.width() == 0) return;

    // Block the update
    LockImageRGB();
    m_u64In++;               // Buffers added
    m_u64InProc--;
    UnlockImageRGB();

    m_pSemUpdate->release(); // Start updating
}
///////////////////////////////////////////////////////////////////////////////////////////
// Create a color table used for spectrogram output
// 3.04.09 - The color table has been given a fixed size to calculate the index of the color table.
//			 The m_lfColorScale coefficient has been introduced based on the spectrum value.
int CSpgramm::CreateColorTable()
{
    if ( Z >= m_AxeDesc.size() ) return 0;

    quint32		dRange = COLOR_TABLE_SIZE;
    double		lfTemp = 0, lfColorMax = 0, lfColorMin = 0, lfMin = 0, lfMax = 0;
    quint32     tmpColor;
    // The scaling factor of the spectrum values in the color table index
    m_lfColorScale = ((double)(dRange - 1)) / fabs(m_AxeDesc[Z].lfMax - m_AxeDesc[Z].lfMin);
    // New minimum and maximum values
    lfMin = m_AxeDesc[Z].lfMin * m_lfColorScale;
    lfMax = m_AxeDesc[Z].lfMax * m_lfColorScale;
    // Recalculate the lower bound of the values, taking into account the use of the color scale
    lfColorMin = lfMin + ((double)dRange)*m_lfPercUseColorsDown;
    lfColorMax = lfMin + ((double)dRange)*m_lfPercUseColorsUp;

    // Create a color table
    if ( dRange != m_colorTable.size() )
    {
        m_colorTable.resize( dRange );
    }

    lfTemp = lfColorMin;
    while( lfTemp < lfColorMax )
    {
        m_colorTable[(int)(lfTemp - lfMin)] = GetColour( lfTemp, lfColorMin, lfColorMax );
        lfTemp += 1.0;
    }
    // Fill the lower unused area of the color scale with the minimum color
    tmpColor = GetColour( lfColorMin, lfColorMin, lfColorMax );
    lfTemp = lfMin;
    while( lfTemp < lfColorMin )
    {
        m_colorTable[(int)(lfColorMin - lfTemp)] = tmpColor;
        lfTemp += 1.0;
    }
    // Fill the upper unused area of the color scale with the maximum color
    tmpColor = GetColour( lfColorMax, lfColorMin, lfColorMax );
    lfTemp = lfColorMax;
    while( lfTemp < lfMax )
    {
        m_colorTable[(int)(lfTemp - lfMin)] = tmpColor;
        lfTemp += 1.0;
    }
    return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::GetColour : The method calculates the color value corresponding to the passed value.
/// \param v        - the value of
/// \param vmin     - minimum value
/// \param vmax     - maximum value
/// \return         - 32-bit ARGB value
quint32 CSpgramm::GetColour(double v, double vmin, double vmax)
{
    quint8	ucR = 255, ucG = 255, ucB = 255; // white
    double dv;

    if (v < vmin)
        v = vmin;

    if (v > vmax)
        v = vmax;

    dv = vmax - vmin;

    if (v < (vmin + 0.25 * dv))
    {
        ucR = 0;
        ucG = (quint8)(255.0 * 4.0 * (v - vmin) / dv);
    }
    else
    if (v < (vmin + 0.5 * dv))
    {
        ucR = 0;
        ucB =  (quint8)(255.0*(1 + 4 * (vmin + 0.25 * dv - v) / dv));
    }
    else
    if (v < (vmin + 0.75 * dv))
    {
        ucR =  (quint8)(255.0 * 4 * (v - vmin - 0.5 * dv) / dv);
        ucB = 0;
    }
    else
    {
        ucG =  (quint8)(255.0 * (1 + 4 * (vmin + 0.75 * dv - v) / dv));
        ucB = 0;
    }

    return (quint32)(ucB + (ucG << 8) + (ucR << 16) + 0xFF000000);//RGB( ucR, ucG, ucB );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SetAxe Set the axis display parameters
// Parameters:
// nAxe		- the ordinal number of the axis from the Axe enumeration
// strName	- Axis name
// strUnit	- axis unit of measurement
// lfMin	- minimum value
// lfMax	- maximum value
// lfStep	- axis unit scale (lfMax is not used)
// Color	- Color for displaying lines and axis text
// dType	- Тip axes (normal, power-law)
// dPrecision - The number of characters displayed after the dot
////////////////////////////////////////////////////////////////////////////////////////////////////
int CSpgramm::SetAxe( Axe nAxe, QString strName, QString strUnit, double lfMin, double lfMax, double lfStep, quint32 lineColor, quint32 textColor, quint16 dType, quint16 dPrecision )
{
    m_AxeDesc[nAxe].qsName = strName;
    m_AxeDesc[nAxe].qsUnit = strUnit;
    m_AxeDesc[nAxe].lfMin = lfMin;
    m_AxeDesc[nAxe].lfMax = lfMax;
    m_AxeDesc[nAxe].lfStep = lfStep;
    m_AxeDesc[nAxe].dType = dType;
    m_AxeDesc[nAxe].dPrecision = dPrecision;
    // Save the color
    m_AxeDesc[nAxe].useTextColor = textColor;
    m_AxeDesc[nAxe].useLineColor = lineColor;

    switch( nAxe )
    {
        case Z:
            CreateColorTable(); // !move to UpdateMinMaxZ
        break;
        case Y:
            qDebug() << "Y scale:" << lfStep;
            m_u16TimePerSecUpd = (quint16)((1.0 / lfStep) / 8.0);   // Updating the time line 8 times per second.
            if( m_u16TimePerSecUpd < 1 ) m_u16TimePerSecUpd = 1;

            m_u16SpgmPerSecUpd = (quint16)((1.0 / lfStep) / (double)(UPDATE_SPGM_PER_SEC) );          // Spectrogram update per second
            if( m_u16SpgmPerSecUpd < 1 ) m_u16SpgmPerSecUpd = 1;
        break;
        case X:
        break;
    }

    return nAxe;
}
///////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::SetLeft : Set the margin on the left
/// \param nLeft
///
void CSpgramm::SetLeft(qint16 nLeft)
{
    m_i16Left = nLeft;
}
///////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::SetRight : Set the margin on the right
/// \param nRight
void CSpgramm::SetRight(qint16 nRight)
{
    m_i16Right = nRight;
}
///////////////////////////////////////////////////////////////////////////////////
// The flow of the spectrogram data update
CThreadUpdate::CThreadUpdate()
{
    m_pParent = 0;
    m_bCancel = true;
    m_u64Out = 0;
}
///////////////////////////////////////////////////////////////////////////////////
// Flow duty cycle
void CThreadUpdate::run()
{
    m_u64Out = 0;
    m_bCancel = false;

    QSemaphore *pSemUpdate = 0;

    if( !m_pParent || !m_pParent->GetSemUpdate() )
        m_bCancel = true;
    else
        pSemUpdate = m_pParent->GetSemUpdate();

    while( !m_bCancel )
    {
        if( pSemUpdate->tryAcquire( 1, 100 ) )
        {
            // A sign that the stream is running and the data is being updated.
            m_pParent->SetPauseUpdate( false );

            m_pParent->LockImageRGB();

            QRect   rectSpgm = m_pParent->GetRectSpgm();
            quint32 u32SpgHeight = rectSpgm.height(), u32SpgWidth = rectSpgm.width();

            if( m_pParent->GetFullUpdate() )
            {
                memset( m_pParent->GetRGB0Data(), 0, rectSpgm.width()*rectSpgm.height()*sizeof(quint32) );
                m_pParent->SetFullUpdate( false );
            }
            quint16         u16CurRd = m_pParent->GetCurRd(), u16CurWr = m_pParent->GetCurWr();
            quint32         u32OrgY = m_pParent->GetOrgY();
            vector<quint32> *vecColorTable  = m_pParent->GetColorTable();
            quint32         u32FFTSize = m_pParent->GetFFTSize();
            double          lfColorScale = m_pParent->GetColorScale();
            vector<AxeDesc> *pvecAxeDesc = m_pParent->GetAxeDesc();

            if( u16CurRd < u16CurWr )
            {
                // Current spectrum
                float           *pBuffer = 0;
                // Get a pointer to an array of points
                quint32         *pRGB0 = m_pParent->GetRGB0Data();
                quint32         *pRGB1 = m_pParent->GetRGB1Data();
                quint32         *pRGB = 0;
                quint8          u8CurRGB = m_pParent->GetCurRGB();
                quint32         u32BytesInStr = u32SpgWidth * sizeof(quint32);
                vector<float*>  *pvecFFT = m_pParent->GetVecFFT();

                // Update the array of points according to the scale
                if( !m_pParent->GetShiftUp2Down() )
                {
                    pRGB = pRGB0;
                    // A new spectrogram is added downwards
                    if( u16CurRd >= u32SpgHeight )
                    {
                        pBuffer = (*pvecFFT)[u32SpgHeight];
                        // Move the image up one line
                        memcpy( pRGB, pRGB + rectSpgm.width(), u32BytesInStr * (u32SpgHeight - 1) );
                        // Pointer to the line where you want to update the image
                        pRGB += u32SpgWidth * (u32SpgHeight - 1); //
                        // Move the 0 buffer to the end of the array
                        float   *pFirstBuffer = (*pvecFFT)[0];
                        pvecFFT->erase( pvecFFT->begin() );
                        pvecFFT->push_back( pFirstBuffer );
                        u32OrgY++;
                        // Not constantly updating
                        if( (u32OrgY % m_pParent->GetTimePerSecUpd() ) == 0 )
                            m_pParent->SetUpdateY( true );
                    }
                    else
                    {
                        pBuffer = (*pvecFFT)[u16CurRd];
                        pRGB += u32SpgWidth * u16CurRd;
                    }
                }
                else
                {
                    // Move the image one line downwards
                    if( !u8CurRGB )
                    {
                        // copy from 0 to 1
                        memcpy( pRGB1 + rectSpgm.width(), pRGB0, u32BytesInStr * (u32SpgHeight - 1) );
                        u8CurRGB = 1;
                    }
                    else
                    {
                        // copy from 1 to 0
                        memcpy( pRGB0 + rectSpgm.width(), pRGB1, u32BytesInStr * (u32SpgHeight - 1) );
                        u8CurRGB = 0;
                    }
                    // Move the last buffer to the beginning of the array
                    pBuffer = (*pvecFFT)[pvecFFT->size() - 1];
                    pvecFFT->erase( pvecFFT->end() - 1 );
                    pvecFFT->insert( pvecFFT->begin(), pBuffer );
                    u32OrgY++;
                    if( (u32OrgY % m_pParent->GetTimePerSecUpd() ) == 0 )
                        m_pParent->SetUpdateY( true );
                    // We always update first
                    pRGB = (!u8CurRGB) ? pRGB0 : pRGB1; // The current buffer for rendering
                    m_pParent->SetCurRGB( u8CurRGB );
                }
                // Refresh the image
                // Converting FFT values to color, saving in an image row
                double  lfScale = (double)(u32SpgWidth - 1) / (double)u32FFTSize;
                quint32 posX = 0, oldX = 0;
                quint32 posFFT = 0;
                qint16  nAccum = (qint16)pBuffer[0], nValue;
                double  lfMinValue = (*pvecAxeDesc)[CSpgramm::Z].lfMin;
                qint32 dColorSize = vecColorTable->size();

                for( posFFT = 0; posFFT < u32FFTSize; posFFT++ )
                {
                    nValue = (qint16)pBuffer[posFFT];
                    if( nAccum < nValue ) nAccum = nValue;  // Calculating the maximum
                    posX = (quint32)((double)posFFT*lfScale);
                    if( oldX < posX )
                    {
                        // Save the color of the accumulated value
                        nAccum = ((double)nAccum - lfMinValue) * lfColorScale;        // Index in the color table
                        quint32 u32Color = 0;
                        if( nAccum < 0 ) nAccum = 0;
                        if( nAccum < dColorSize )
                            u32Color = (*vecColorTable)[nAccum];
                        else
                            u32Color = (*vecColorTable)[dColorSize - 1];

                        while( oldX < posX )
                        {
                            // Save the color of the accumulated value
                            pRGB[oldX] = u32Color;
                            oldX++;
                        }
                        // Initialize the adder
                        nAccum = nValue;
                    }
                }
                // Save the color of the accumulated value for the extreme point
                nAccum = ((double)nAccum - lfMinValue) * lfColorScale;        // Index in the color table
                if( nAccum < 0 ) nAccum = 0;
                if( nAccum < dColorSize )
                    pRGB[oldX] = (*vecColorTable)[nAccum];
                else
                    pRGB[oldX] = (*vecColorTable)[dColorSize - 1];
                // End of image line formation
                u16CurRd++; // Next buffer
                m_u64Out++;
            }

            if( u16CurRd > u32SpgHeight ) u16CurRd = u32SpgHeight;
            if( u16CurWr > u32SpgHeight ) u16CurWr--;

            m_pParent->SetCurRd( u16CurRd );
            m_pParent->SetCurWr( u16CurWr );
            m_pParent->SetOrgY( u32OrgY );

            m_pParent->UnlockImageRGB();

            if( (u32OrgY % m_pParent->GetSpgmPerSecUpd()) == 0)
            {
                emit sigUpdate();
            }
        }
        else
            m_pParent->SetPauseUpdate( true );
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Updating all axes and spectrogram
void CSpgramm::FullUpdate()
{
    SetFullUpdate( true );
    m_bUpdateX = true;

    // If the spectrogram update is stopped
    if( m_bPauseUpdate )
    {
        // Start over
        SetCurWr( 0 );
        SetCurRd( 0 );
        SetOrgY( 0 );

        onUpdate();
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::onUpdate   : Update spectrogram
void CSpgramm::onUpdate()
{
    m_mutexUpdate.lock();

    repaint();

    m_mutexUpdate.unlock();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::UpdateAxeY : Updating the time axis
void CSpgramm::UpdateAxeY()
{
    SetUpdateY( true );
    onUpdate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::mousePressEvent : Mouse click signal handler
/// \param event
void CSpgramm::mousePressEvent(QMouseEvent *event)
{
    if( event->button() == Qt::LeftButton )
    {// Left-click
        QPoint  ptPosition = event->pos();

        if( m_rectSpgm.contains( ptPosition ) )
        {
            // Send a click signal on the spectrogram
            emit sigClickOnSpgramm();

            if( m_bMrk1 )   // Update marker 1 values
            {
                m_ptPosMrk1 = ptPosition;
                m_bMrk1 = false;
            }
            else
            if( m_bMrk2 )   // Update marker 2 values
            {
                m_ptPosMrk2 = ptPosition;
                m_bMrk2 = false;
            }
            else
            {// Initial scaling position
                m_ptPosScale1 = ptPosition;
            }

            if( m_bPauseUpdate )
            { // Spectrogram data is not updating
                // Update spectrogram
                onUpdate();
                updateMrk();
            }
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::mouseReleaseEvent : Mouse button release signal handler
/// \param event - Event Information
///
void CSpgramm::mouseReleaseEvent(QMouseEvent *event)
{
    if( !m_bMrk1 && !m_bMrk2 && m_ptPosScale1.x() != -1 )
    { // If markers are not used and the starting position is set
        QPoint  ptPosition = event->pos();

        if( m_rectSpgm.contains( ptPosition ) )
        {
            m_ptPosScale2 = ptPosition; // Final scaling position
            // Perform scaling
            if( m_bPauseUpdate && m_ptPosScale1 != m_ptPosScale2 &&     // Check additional terms
                    abs(m_ptPosScale1.x() - m_ptPosScale2.x()) > 2 &&
                    abs(m_ptPosScale1.y() - m_ptPosScale2.y()) > 2 )
                Scale();
            else
            {// Do not perform scaling
                m_ptPosScale1 = QPoint( -1, -1 );
                m_ptPosScale2 = QPoint( -1, -1 );
            }
        }
        else
        {
            m_ptPosScale1 = QPoint( -1, -1 );
            m_ptPosScale2 = QPoint( -1, -1 );
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::mouseMoveEvent : Mouse cursor movement signal handler
/// \param event - Event Information
void CSpgramm::mouseMoveEvent(QMouseEvent *event)
{
    QPoint  ptPosition = event->pos(), *pptMrk = 0;
    // Update current marker information
    if( m_bMrk1 )
        pptMrk = &m_ptPosMrk1;
    else
    if( m_bMrk2 )
        pptMrk = &m_ptPosMrk2;

    if( m_rectSpgm.contains( ptPosition ) )
    { // Cursor in the spectrogram area
        if( pptMrk )
            *pptMrk = ptPosition;
        else
        {
            // If the marker is not selected, update the cursor position information
            m_ptPosScale2 = ptPosition;
        }

        m_ptPosMrk = ptPosition;

        if( m_bPauseUpdate )
        {
            // If the spectrogram data is not updated
            onUpdate();     // Update marker image
            updateMrk();
        }
        else
            updateMrk();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// On/Off marker 1
void CSpgramm::OnMrk1( bool bState )
{
    m_bMrk1 = bState;
    if( m_bMrk1 ) m_bMrk2 = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// On/Off marker 2
void CSpgramm::OnMrk2( bool bState )
{
    m_bMrk2 = bState;
    if( m_bMrk2 ) m_bMrk1 = false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update values ​​at marker points
void CSpgramm::updateMrk()
{
    if( m_AxeDesc.size() <= X || m_AxeDesc.size() <= Y ) return;
    if( m_u16CurScale >= m_vecScale.size() ) return;

    int     nPosX1 = -1, nPosY1 = -1;       // Coordinates of marker 1
    int     nPosX2 = -1, nPosY2 = -1;       // Coordinates of marker 2
    double  lfTime1 = 0.0, lfTime2 = 0.0;   // Time
    double  lfFreq1 = 0.0, lfFreq2 = 0.0;   // Frequency
    double	lfValueScaleX = 1.0;            // Scale for single image
    double	lfValueScaleY = 1.0;
    int     iSpHeight = m_rectSpgm.height(), iSpWidth = m_rectSpgm.width();
    quint16 u16StartY = m_vecScale[m_u16CurScale].nStartY;
    double  lfScaleY = (double)(m_vecScale[m_u16CurScale].nEndY - m_vecScale[m_u16CurScale].nStartY) / (double)(iSpHeight - 1);
    int     nPosY = 0;
    quint16 u16StartX = m_vecScale[m_u16CurScale].nStartX;
    double  lfScaleX = (double)(m_vecScale[m_u16CurScale].nEndX - m_vecScale[m_u16CurScale].nStartX) / (double)(iSpWidth - 1);
    quint16 nPosFFT = 0;
    float   *pBuffer = 0;

    switch ( m_AxeDesc[X].dType )
    {
        case LINEAR_FACTOR_SCALE:	lfValueScaleX = m_AxeDesc[X].lfStep;
            break;
        default:
            lfValueScaleX = (m_AxeDesc[X].lfMax - m_AxeDesc[X].lfMin) / (m_u32FFTSize - 1);
    }

    switch ( m_AxeDesc[Y].dType )
    {
    case LINEAR_FACTOR_SCALE:
        lfValueScaleY = m_AxeDesc[Y].lfStep;
        break;
    default: lfValueScaleY = (m_AxeDesc[Y].lfMax - m_AxeDesc[Y].lfMin) / (m_rectSpgm.height() - 1);	// Масштаб для ед.изобр
    }

    if( m_ptPosMrk1.x() != -1 )
    {
        nPosX1 = m_ptPosMrk1.x() - m_rectSpgm.left();
        nPosY1 = m_ptPosMrk1.y() - m_rectSpgm.top();

        nPosFFT = (quint16)((double)(nPosX1 * lfScaleX)) + u16StartX;

        switch ( m_AxeDesc[X].dType )
        {
            case LINEAR_FACTOR_SCALE:// The scale factor was calculated earlier
            case LINEAR_SCALE:
                // pos.on screen -> unit of image -> value
                lfFreq1 = (double)nPosFFT * lfValueScaleX + m_AxeDesc[X].lfMin;
                break;			// Linear value
            //case EXP_SCALE: lfFreq1 = (double)( pow(m_AxeDesc[X].lfStep, floor( (double)nPosX1 / lfScaleX ) ) * m_AxeDesc[X].lfMin);			// Exponential scale
            //    break;			//
        }

        nPosY = (double)nPosY1*lfScaleY + u16StartY; // Position taking into account scale
        lfTime1 = 0.0;
        if( !m_bShiftUp2Down )
        {
            lfTime1 = (double)((double)nPosY + (double)m_u32OrgY) * lfValueScaleY + m_AxeDesc[Y].lfMin;
        }
        else
        {
            double lfPos = (double)m_u32OrgY - (double)nPosY;
            if( lfPos >= 0 )
                lfTime1 = (double)(lfPos) * lfValueScaleY + m_AxeDesc[Y].lfMin;
        }

        if( nPosY >= 0 && nPosY < static_cast<int>(m_vecFFTs.size()) )
            pBuffer = m_vecFFTs[nPosY];

        emit sigUpdatedMrk1( lfFreq1, lfTime1 );
    }

    if( m_ptPosMrk2.x() != -1 )
    {
        nPosX2 = m_ptPosMrk2.x() - m_rectSpgm.left();
        nPosY2 = m_ptPosMrk2.y() - m_rectSpgm.top();

        nPosFFT = (quint16)((double)(nPosX2 * lfScaleX)) + u16StartX;

        switch ( m_AxeDesc[X].dType )
        {
            case LINEAR_FACTOR_SCALE://The scale factor was calculated earlier
            case LINEAR_SCALE:
                // pos.on screen -> unit of image -> value
                lfFreq2 = (double)nPosFFT * lfValueScaleX + m_AxeDesc[X].lfMin;
                break;			// Linear value
            //case EXP_SCALE: lfFreq2 = (double)( pow(m_AxeDesc[X].lfStep, floor( (double)nPosX2 / lfScaleX ) ) * m_AxeDesc[X].lfMin);			// Exponential scale
            //    break;
        }

        nPosY = (double)nPosY2*lfScaleY + u16StartY; // Position taking into account scale
        lfTime2 = 0.0;
        if( !m_bShiftUp2Down )
        {
            lfTime2 = (double)((double)nPosY + (double)m_u32OrgY) * lfValueScaleY + m_AxeDesc[Y].lfMin;
        }
        else
        {
            double lfPos = (double)m_u32OrgY - (double)nPosY;
            if( lfPos >= 0 )
                lfTime2 = (double)(lfPos) * lfValueScaleY + m_AxeDesc[Y].lfMin;
        }

        if( nPosY >= 0 && nPosY < static_cast<int>(m_vecFFTs.size()) )
            pBuffer = m_vecFFTs[nPosY];

        emit sigUpdatedMrk2( lfFreq2, lfTime2 );
    }

    if( m_ptPosMrk1.x() == -1 && m_ptPosMrk2.x() == -1 && m_ptPosMrk.x() != -1)
    {
        nPosX1 = m_ptPosMrk.x() - m_rectSpgm.left();
        nPosY1 = m_ptPosMrk.y() - m_rectSpgm.top();

        nPosFFT = (quint16)((double)(nPosX1 * lfScaleX)) + u16StartX;

        switch ( m_AxeDesc[X].dType )
        {
            case LINEAR_FACTOR_SCALE:
            case LINEAR_SCALE:
                lfFreq1 = (double)nPosFFT * lfValueScaleX + m_AxeDesc[X].lfMin;
                break;
        }

        nPosY = (double)nPosY1*lfScaleY + u16StartY; // Position taking into account scale
        lfTime1 = 0.0;
        if( !m_bShiftUp2Down )
        {
            lfTime1 = (double)((double)nPosY + (double)m_u32OrgY) * lfValueScaleY + m_AxeDesc[Y].lfMin;
        }
        else
        {
            double lfPos = (double)m_u32OrgY - (double)nPosY;
            if( lfPos >= 0 )
                lfTime1 = (double)(lfPos) * lfValueScaleY + m_AxeDesc[Y].lfMin;
        }

        emit sigUpdatedMrk1( lfFreq1, lfTime1 );
    }

    if( pBuffer )
        emit sigUpdateFFT( pBuffer );
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Deactivate all markers
void CSpgramm::ResetMrks()
{
    m_bMrk1 = false;
    m_bMrk2 = false;
    m_ptPosMrk1 = QPoint(-1, -1);        // Positions of markers 1 and 2
    m_ptPosMrk2 = QPoint(-1, -1);
    m_ptPosScale1 = QPoint(-1, -1);
    m_ptPosScale2 = QPoint(-1, -1);

    emit sigUpdatedMrk1( 0, 0 );
    emit sigUpdatedMrk2( 0, 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scaling
void CSpgramm::Scale()
{
    // Current scale
    int     iSpHeight = m_rectSpgm.height(), iSpWidth = m_rectSpgm.width();
    quint16 u16StartY = m_vecScale[m_u16CurScale].nStartY;
    double  lfScaleY = (double)(m_vecScale[m_u16CurScale].nEndY - m_vecScale[m_u16CurScale].nStartY) / (double)(iSpHeight - 1);
    quint32 u32StartX = m_vecScale[m_u16CurScale].nStartX;
    double  lfScaleX = (double)(m_vecScale[m_u16CurScale].nEndX - m_vecScale[m_u16CurScale].nStartX) / (double)(iSpWidth - 1);
    int     nTmp;

    // Preparing data for the next scaling
    m_u16CurScale++;
    if( m_vecScale.size() <= m_u16CurScale ) m_vecScale.resize( m_u16CurScale + 1 );

    // Convert coordinates to the index of the spectrogram element
    m_vecScale[m_u16CurScale].nStartX = (int)((double)(m_ptPosScale1.x() - m_rectSpgm.left()) * lfScaleX) + u32StartX;
    m_vecScale[m_u16CurScale].nEndX = (int)((double)(m_ptPosScale2.x() - m_rectSpgm.left()) * lfScaleX) + u32StartX;
    if( m_vecScale[m_u16CurScale].nStartX > m_vecScale[m_u16CurScale].nEndX )
    {// Swap, Start should be smaller
        nTmp = m_vecScale[m_u16CurScale].nStartX;
        m_vecScale[m_u16CurScale].nStartX = m_vecScale[m_u16CurScale].nEndX;
        m_vecScale[m_u16CurScale].nEndX = nTmp;
    }
    // For Y, the coordinates and the index of the spectrogram array are the same
    m_vecScale[m_u16CurScale].nStartY = (int)((double)(m_ptPosScale1.y() - m_rectSpgm.top())*lfScaleY) + u16StartY;
    m_vecScale[m_u16CurScale].nEndY = (int)((double)(m_ptPosScale2.y() - m_rectSpgm.top())*lfScaleY) + u16StartY;

    if( m_vecScale[m_u16CurScale].nStartY > m_vecScale[m_u16CurScale].nEndY )
    { // Swap, Start should be smaller
        nTmp = m_vecScale[m_u16CurScale].nStartY;
        m_vecScale[m_u16CurScale].nStartY = m_vecScale[m_u16CurScale].nEndY;
        m_vecScale[m_u16CurScale].nEndY = nTmp;
    }

    if( m_vecScale[m_u16CurScale].nEndY - m_vecScale[m_u16CurScale].nStartY < 3 ||
        m_vecScale[m_u16CurScale].nEndX - m_vecScale[m_u16CurScale].nStartX < 3 )
    {// Too big scale, nothing to do
        if( m_u16CurScale > 0 ) m_u16CurScale--;
    }
    else
    {
        // Refresh the spectrogram image
        PrepareSpgmImg();

        SetUpdateY( true );
        m_bUpdateX = true;
    }

    ResetMrks();
    onUpdate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Temporary: update completely
void CSpgramm::RedrawAll()
{
    QRect   rectSpgm = GetRectSpgm();
    quint32 u32SpgHeight = rectSpgm.height();

    PrepareSpgmImg( u32SpgHeight ); // Update one spectrogram screen

    ResetMrks();
    onUpdate();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// resizeEvent event handler
void CSpgramm::resizeEvent(QResizeEvent *event)
{
    if( !m_pLinePen )
    { // Initialize brushes
        m_pLinePen = 0;
        m_pLinePen = new QPen( m_lineColor );
        if( m_pLinePen ) { m_pLinePen->setStyle(Qt::SolidLine); m_pLinePen->setWidth(1); }

        m_pTextPen = 0;
        m_pTextPen = new QPen( m_textColor );
        if( m_pTextPen ) { m_pTextPen->setStyle(Qt::SolidLine); m_pTextPen->setWidth(1); }

        m_pMrk1Pen = 0;
        m_pMrk1Pen = new QPen( QColor( 255, 255, 255 ) );
        if( m_pMrk1Pen ) { m_pMrk1Pen->setStyle(Qt::DashLine); m_pMrk1Pen->setWidth(1); }

        m_pMrk2Pen = 0;
        m_pMrk2Pen = new QPen( QColor( 255, 255, 255 ) );
        if( m_pMrk2Pen ) { m_pMrk2Pen->setStyle(Qt::DashLine); m_pMrk2Pen->setWidth(1); }
    }

    if( event->size().width() == 0 || event->size().height() == 0 ) return;

    if( m_qsizeClient.width() !=  event->size().width() || m_qsizeClient.height() != event->size().height() )
    { // If the size has changed
        ResizeSpgm( event->size() ); // Change the spectrogram size
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief CSpgramm::ResizeSpgm
/// Change spectrogramm size
void CSpgramm::ResizeSpgm( QSize qsizeNew )
{
    quint32 u32Points = 0;

    LockImageRGB();

    if( qsizeNew.width() != 0 && qsizeNew.height() != 0 )
        m_qsizeClient = qsizeNew;                    // Keep previous size
    QFontMetrics tmpFontMetric( m_defFont );

    m_i16Bottom = (float)tmpFontMetric.height() * 1.5;      // Top and bottom indents are 2.5 times the font height.
    if( !m_bDrawTitle )
        m_i16Top = 1;                                       // There is only a limiting line at the top
    else
        m_i16Top = m_i16Bottom;                             // The title text is displayed at the top

    if( m_i16Left < m_i16Bottom ) m_i16Left = m_i16Bottom;  // Minimum distance for displaying Y-axis labels
    if( m_i16Right < m_i16Bottom ) m_i16Right = m_i16Bottom;// Should be equal to the growth from the left

    QMargins    spgmMargins = QMargins( m_i16Left, m_i16Top, m_i16Right, m_i16Bottom );

    m_rectSpgm.setX( 0 );
    m_rectSpgm.setY( 0 );
    m_rectSpgm.setSize( m_qsizeClient );
    m_rectSpgm -= spgmMargins;

    if( m_rectSpgm.width() > 10 && m_rectSpgm.height() > 10 )
    {
        u32Points = m_rectSpgm.width() * m_rectSpgm.height();
        // Prepare spectrogram buffer #1
        m_vecRGB_0.resize( u32Points );
        memset( m_vecRGB_0.data(), 0, u32Points*sizeof(quint32) );
        m_imageRGB_0 = QImage( (quint8*)(m_vecRGB_0.data()), m_rectSpgm.width(), m_rectSpgm.height(), QImage::Format_RGB32 );
        // Prepare spectrogram buffer #2
        m_vecRGB_1.resize( u32Points );
        memset( m_vecRGB_1.data(), 0, u32Points*sizeof(quint32) );
        m_imageRGB_1 = QImage( (quint8*)(m_vecRGB_1.data()), m_rectSpgm.width(), m_rectSpgm.height(), QImage::Format_RGB32 );
        // Original scale
        m_vecScale[0].nStartX = 0;
        m_vecScale[0].nEndX = m_u32FFTSize - 1;
        m_vecScale[0].nStartY = 0;
        m_vecScale[0].nEndY = m_rectSpgm.height() - 1;
        // Prepare images of the axes
        if( m_bDrawTitle )
            PrepareImageTopX();

        PrepareImageBotX();
        PrepareImageLeftY();
        PrepareImageRightY();

        // Start over
        SetCurWr( 0 );
        SetCurRd( 0 );
        SetOrgY( 0 );
        SetUpdateY( true );
    }

    UnlockImageRGB();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Drawing a spectrogram from the buffer
void CSpgramm::PrepareSpgmImg( quint16 u32UpdateRows )
{
    if( m_u16CurScale >= m_vecScale.size() ) return;
    if( m_AxeDesc.size() <= X || m_AxeDesc.size() <= Y ) return;

    quint16 u16CurRd = m_u16CurRd;
    quint32 *pRGB = (!m_u8CurRGB) ? m_vecRGB_0.data() : m_vecRGB_1.data(), *pRGBOrg = 0;
    quint16 u16Width = m_rectSpgm.width(), u16Height = m_rectSpgm.height(), u16Y = 0;
    quint32 u32StartX = m_vecScale[m_u16CurScale].nStartX, u32EndX = m_vecScale[m_u16CurScale].nEndX;
    quint16 u16StartY = m_vecScale[m_u16CurScale].nStartY;
    double  lfScaleY = (double)(m_vecScale[m_u16CurScale].nEndY - m_vecScale[m_u16CurScale].nStartY) / (double)(u16Height - 1);
    double  lfScaleX = (double)(u16Width - 1) / (double)(m_vecScale[m_u16CurScale].nEndX - m_vecScale[m_u16CurScale].nStartX);
    // Current spectrum
    float   *pBuffer = 0;
    double  lfMinValue = m_AxeDesc[Z].lfMin;
    qint32 dColorSize = m_colorTable.size();
    // Clear the spectrogram
    memset( pRGB, 0, m_vecRGB_0.size()*sizeof(quint32) );

    if( u32UpdateRows > 0 ) // Update the specified number of spectrograms
        u16CurRd = u32UpdateRows;

    for( u16Y = 0; u16Y < u16Height; u16Y++ )
    {
        // Drawing an array of spectrograms
        quint16 u16PosInBuffers = u16Y*lfScaleY + u16StartY; // spectrum index from nStartY to nEndY
        pRGBOrg = pRGB + u16Width*u16Y;                      // position of the beginning of the spectrum image

        if( u16PosInBuffers >= m_vecFFTs.size() )
        {
            qDebug() << "m_vecFFTs.size()" << m_vecFFTs.size() << "u16PosInBuffers:" << u16PosInBuffers;
            break;
        }

        if( u16PosInBuffers >= u16CurRd )
        {
            // No data, clear image
            memset( pRGBOrg, 0, u16Width*sizeof(quint32) );
        }
        else
        {
            pBuffer = m_vecFFTs[u16PosInBuffers];
            // Drawing a spectrogram line
            qint16  nAccum = (qint16)pBuffer[u32StartX], nValue;
            quint32 posX = 0, oldX = 0;
            quint32 posFFT = 0;
            for( posFFT = u32StartX; posFFT <= u32EndX; posFFT++ )
            {
                nValue = (qint16)pBuffer[posFFT];
                if( nAccum < nValue ) nAccum = nValue;  // We calculate the maximum
                posX = (quint32)((double)(posFFT - u32StartX)*lfScaleX);
                if( posX >= u16Width )
                {
                    qDebug() << "u16Y:" << u16Y << " posX:" << posX << " u16Width:" << u16Width;
                    break;
                }

                if( oldX < posX )
                {
                    // Save the color of the accumulated value
                    nAccum = ((double)nAccum - lfMinValue) * m_lfColorScale;        // Index in the color table
                    quint32 u32Color = 0;
                    if( nAccum < 0 ) nAccum = 0;
                    if( nAccum < dColorSize )
                        u32Color = m_colorTable[nAccum];
                    else
                        u32Color = m_colorTable[dColorSize - 1];

                    while( oldX < posX )
                    {
                        // Save the color of the accumulated value
                        pRGBOrg[oldX] = u32Color;
                        oldX++;
                    }
                    // Initialize
                    nAccum = nValue;
                }
            }
        }
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// Set original scale
void CSpgramm::ResetScale()
{
    if( m_u16CurScale > 0 ) m_u16CurScale = 0;
    m_u64In = 0;
    m_u64Out = 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// Return to previous zoom
void CSpgramm::DecScale()
{
    if( m_u16CurScale > 0 )
    {
        m_u16CurScale--;
        PrepareSpgmImg();
        ResetMrks();

        SetUpdateY(true);
        m_bUpdateX = true;
        onUpdate();
    }
}
