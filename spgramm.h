#ifndef QSPGRAMM_H
#define QSPGRAMM_H

#include <QWidget>
#include <QMutex>
#include <vector>
#include <QThread>
#include <QSemaphore>

using namespace std;

#define DEFAULT_ORIGIN          20      // Offset from the edge of the widget area (default value)
#define	COLOR_TABLE_SIZE		16384   // The size of the color table

// Types of scales used for axes
#define	LINEAR_SCALE			0       // Linear, calculation of the coefficient by min. and max.
#define	LINEAR_FACTOR_SCALE		1       // Linear, the coefficient is specified explicitly
#define	EXP_SCALE				2       // Exponential scale, the coefficient is used as an indicator of the degree

#define	SCALE_STEP              3       // The output step of the scale gaps (in pixels)
#define	DIV_SIZE				8       // The size of the line to display the division on the scale

#define UPDATE_SPGM_PER_SEC     100     // Spectrogram updates per second

#pragma pack(1)
// Structure for storing axis parameters
typedef struct tagAxeDesc
{
    tagAxeDesc(){// Default Constructor
        qsName.clear();
        qsUnit.clear();
        lfMin = lfMax = 0.0;
        lfStep = 1.0;
        dType = 0;
        dPrecision = 0;
        useTextColor =  QColor( 0, 0, 0, 255 );
        useLineColor = QColor( 0, 0, 0, 255 );
    };

    ~tagAxeDesc(){
    };

    QString		qsName, qsUnit;				// Name, unit of measurement
    double		lfMin, lfMax, lfStep;		// min., max. values, scale factor
    quint16     dType, dPrecision;          // Scale type, number of decimal places displayed
    QColor		useTextColor, useLineColor;	// Current color
}AxeDesc;

// Information required for scaling
typedef struct tagScaleInf
{
    tagScaleInf()
    {
        nStartX = nEndX = -1;
        nStartY = nEndY = -1;
        lfScaleX = lfScaleY = 1.0;
    };
    tagScaleInf( const tagScaleInf& Src )
    {
        nStartX = Src.nStartX;
        nEndX = Src.nEndX;
        nStartY = Src.nStartY;
        nEndY = Src.nEndY;
        lfScaleX = Src.lfScaleX;
        lfScaleY = Src.lfScaleY;
    };
    tagScaleInf& operator=( const tagScaleInf& Src )
    {
        nStartX = Src.nStartX;
        nEndX = Src.nEndX;
        nStartY = Src.nStartY;
        nEndY = Src.nEndY;
        lfScaleX = Src.lfScaleX;
        lfScaleY = Src.lfScaleY;
        return *this;
    };

    int nStartX, nEndX;         // The beginning and end of the area, X coordinate
    int nStartY, nEndY;         // The beginning and end of the area, Y coordinate
    double lfScaleX, lfScaleY;  // Scale by: X and Y
}ScaleInf;

#pragma pack()

class CSpgramm;
//////////////////////////////////////////////////////////////////////////////
// The flow of the spectrogram data update
class CThreadUpdate: public QThread
{
    Q_OBJECT

public:
    CThreadUpdate();

    void SetParent( CSpgramm   *pParent ) { m_pParent = pParent; }; // Set the pointer to the spectrogram widget
    void StopThread() { m_bCancel = true; };                        // Stop the flow
    bool IsRunning() { return !m_bCancel; };                        // Return the flag of the flow operation
    quint64 GetOutCount() { return m_u64Out; };                     // Return the number of "processed" buffers

    void run();

signals:
    void sigUpdate();

protected:
    bool            m_bCancel;                                      // Indicates that the thread is shutting down
    CSpgramm        *m_pParent;                                     // Pointer to the spectrogram widget
    quint64         m_u64Out;                                       // Number of "processed" buffers
};

class CSpgramm : public QWidget
{
    Q_OBJECT
public:
    explicit CSpgramm(QWidget *parent = nullptr);
    ~CSpgramm();

    typedef enum{ X = 0, Y = 1, Z = 2}Axe;          // Ordinal numbers corresponding to the axes

    void SetFFTSize(quint32 u32FFTSize);            // Initialize an array of spectra
    quint32  GetFFTSize() { return m_u32FFTSize; }; // Return the current size of the unit spectrum

    float *GetFFTBuffer();                          // Return the pointer to the current spectrum buffer
    void AddFFTBuffer();                            // Update the data of the current spectrum buffer
    // Set the axis display parameters
    int SetAxe(Axe nAxe, QString strName, QString strUnit, double lfMin, double lfMax, double lfStep, quint32 lineColor, quint32 textColor, quint16 dType, quint16 dPrecision);
    void SetLeft( qint16 nLeft );                   // Set the margin on the left
    void SetRight(qint16 nRight );                  // Set the margin on the right
    void ResizeSpgm(QSize qsizeNew = QSize( 0, 0 ));
    void FullUpdate();                              // Updating all axes
    void UpdateAxeY();                              // Updating the time axis
    void OnMrk1( bool bState );                     // On/off Marker  1
    void OnMrk2( bool bState );                     // On/off Marker  2
    void ResetMrks();                               // Remove markers from the screen
    void ResetScale();                              // Set the default scale
    void DecScale();                                // Go back to the previous scale
    // Semaphore for updating the original spectrogram data
    QSemaphore* GetSemUpdate() { return m_pSemUpdate; };
    // Set the title text
    void SetTitle( QString qsTitle ) { m_qsTitle = qsTitle; };
    // On/Off header output
    void SetDrawTitle( bool bDrawTitle ) { m_bDrawTitle = bDrawTitle; };
    // Set the spectrogram shift direction from top to bottom
    void SetShiftUp2Down( bool bShiftUp2Down ) { m_bShiftUp2Down = bShiftUp2Down; };
    // Return the current shift direction
    bool GetShiftUp2Down() { return m_bShiftUp2Down; };
    // Methods for accessing properties from a stream ///////////////////////////////////////////////////////////////////////////////
    bool GetFullUpdate() { return m_bFullUpdate; };         // Restore the full update flag
    // Enable full update
    void SetFullUpdate( bool bFullUpdate ) { m_mutexSet.lock(); m_bFullUpdate = bFullUpdate; m_u8CurRGB = 0; m_mutexSet.unlock(); };
    bool GetPauseUpdate() { return m_bPauseUpdate; };       // Возвращает признак прекращения обновления данных
    // Sets the flag for stopping data updates.
    void SetPauseUpdate( bool bPauseUpdate ) { m_bPauseUpdate = bPauseUpdate; };
    void LockImageRGB() { m_mutexWrite.lock(); };           // Block update
    void UnlockImageRGB() { m_mutexWrite.unlock(); };       // Unlock the update
    QRect GetRectSpgm() { return m_rectSpgm; };             // Return the coordinates of the spectrogram region
    //
    quint32* GetRGB0Data() { return m_vecRGB_0.data(); };   // Return the image array of spectrogram #1
    quint32* GetRGB1Data() { return m_vecRGB_1.data(); };   // Return the image array of spectrogram #2
    // Position of the current spectrum buffer
    quint16 GetCurWr() { return m_u16CurWr; };
    void SetCurWr( quint16 u16CurWr ) { m_mutexSet.lock(); m_u16CurWr = u16CurWr; m_mutexSet.unlock(); };
    // Position of the updated spectrum buffer
    quint16 GetCurRd() { return m_u16CurRd; };
    void SetCurRd( quint16 u16CurRd ) { m_mutexSet.lock(); m_u16CurRd = u16CurRd; m_mutexSet.unlock(); };
    //
    vector<float*>* GetVecFFT() { return &m_vecFFTs; };     // Spectral buffer
    //
    quint32 GetOrgY() { return m_u32OrgY; };                // Return the number of the updated spectrogram row
    // Set the number of the updated spectrogram row
    void SetOrgY( quint32 u32OrgY ) {  m_mutexSet.lock(); m_u32OrgY = u32OrgY;  m_mutexSet.unlock(); };
    // Return color table
    vector<quint32>* GetColorTable() { return &m_colorTable; };
    bool GetUpdateY() { return m_bUpdateY; };               // Return the Y-axis update flag
    // Set the Y-axis update flag
    void SetUpdateY( bool bUpdateY ) { m_mutexSet.lock(); m_bUpdateY = bUpdateY; m_mutexSet.unlock(); };
    // Return the time axis refresh rate
    quint16  GetTimePerSecUpd() { return m_u16TimePerSecUpd; };
    // Return the spectrogram refresh rate
    quint16  GetSpgmPerSecUpd() { return m_u16SpgmPerSecUpd; };
    // Return the scale of a value in a color table
    double GetColorScale() { return m_lfColorScale; };
    //
    quint8 GetCurRGB() { return m_u8CurRGB; };              // Return the current spectrogram image buffer
    // Set the current spectrogram image buffe
    void SetCurRGB( quint8 u8CurRGB ) { m_u8CurRGB = u8CurRGB; };
    // Параметры осей
    vector<AxeDesc>* GetAxeDesc() { return &m_AxeDesc; };
    void RedrawAll();

signals:
    void sigClickOnSpgramm();                               // Click on the spectrogram
    void sigUpdatedMrk1( double lfX, double lfY );          // The position of marker 1 has been updated.
    void sigUpdatedMrk2( double lfX, double lfY );          // The position of marker 2 has been updated.
    void sigUpdateFFT( float *pBuffer );                    // The current FFT has been updated

public slots:
    void onUpdate();                                    // Thread-safe call to update spectrogram

protected:
    // Margins: top, left, bottom, right
    qint16              m_i16Top, m_i16Left, m_i16Bottom, m_i16Right;
    QRect               m_rectSpgm;                     // Spectrogram area
    QSize               m_qsizeClient;                  // Widget client area
    // Refresh: X-axis, Y-axis, full, spectrogram not refreshing
    bool                m_bUpdateY, m_bUpdateX, m_bFullUpdate, m_bPauseUpdate;
    quint16             m_u16TimePerSecUpd, m_u16SpgmPerSecUpd;
    vector<float*>      m_vecFFTs;                      // Spectral buffer
    vector<quint32>     m_vecRGB_0, m_vecRGB_1;         // Spectrogram image
    QImage              m_imageRGB_0, m_imageRGB_1;     // Spectrogram display class object
    quint8              m_u8CurRGB;                     // Current spectrogram image
    quint32             m_u32FFTSize;                   // Current FFT size
    quint16             m_u16CurWr, m_u16CurRd;         // Current index in the FFT buffer
    vector<quint32>     m_colorTable;                   // Color chart for meanings
    vector<AxeDesc>     m_AxeDesc;                      // Axle parameters
    double              m_lfColorScale;                 // scaling factor in the color table
    // Upper and lower limits for color table usage in percentages
    double              m_lfPercUseColorsDown, m_lfPercUseColorsUp;
    QColor				m_bkColor, m_textColor, m_lineColor;
    QPen				*m_pLinePen, *m_pTextPen, *m_pMrk1Pen, *m_pMrk2Pen;
    QMutex              m_mutexWrite;                   // Blocking concurrent buffer modification
    QMutex              m_mutexUpdate;                  // Blocking simultaneous spectrogram updates
    QMutex              m_mutexSet;                     // Blocking simultaneous setting of values
    QImage              *m_pTopImage, *m_pBotImage, *m_pLeftImage, *m_pRightImage;
    QFont               m_defFont;
    quint32             m_u32OrgY;                      // Y-axis shift (number of FFTs removed)
    QPoint              m_ptPosMrk1;                    // Positions of markers 1 and 2
    QPoint              m_ptPosMrk2;
    QPoint              m_ptPosMrk;                     // Position without marker
    bool                m_bMrk1, m_bMrk2;               // Enable marker 1 and marker 2 output
    // Scaling
    QPoint              m_ptPosScale1;                  // Starting point
    QPoint              m_ptPosScale2;                  // The end point
    vector<ScaleInf>    m_vecScale;                     // Array of information for scaling
    quint16             m_u16CurScale;                  // The current element in the array
    // Statistics, temporary
    quint64             m_u64In, m_u64Out, m_u64InProc, m_u64InProcMax;
    // Shift
    bool                m_bShiftUp2Down;                // Above is the "fresh" data
    bool                m_bDrawTitle;                   // Header output
    QString             m_qsTitle;                      // Headline text
    // Data update flow ///////////////////////////////////////////////////
    QSemaphore          *m_pSemUpdate;                  // Spectrogram data availability semaphore
    CThreadUpdate       m_qthreadUpdate;                // Spectrogram image update stream

protected:
    void resizeEvent(QResizeEvent *event);                  // Element resize event handler
    void paintEvent(QPaintEvent *event);                    // Element render event handler
    void ReleaseFFTBuffers();                               // Free up spectral array memory
    int CreateColorTable();                                 // Create a color chart
    quint32 GetColour(double v, double vmin, double vmax);  // Return the color value
    void ReleasePainterMem();                               // Release brushes, pencils, etc.
    void PrepareImageTopX( bool bReleaseMem = true );       // X-axis image from above
    void PrepareImageBotX( bool bReleaseMem = true );       // X-axis image from below
    void PrepareImageLeftY();                               // Y-axis image on the left
    void PrepareImageRightY();                              // Y-axis image on the right
    void ReduceValue(QString &qsFormat, double &lfValue);
    void DrawLeftYText();
    // Mouse event handlers
    void mousePressEvent(QMouseEvent *event);               // Mouse click signal handler
    void mouseReleaseEvent(QMouseEvent *event);             // Mouse button release signal handler
    void mouseMoveEvent(QMouseEvent *event);                // Mouse cursor movement signal handler
    void updateMrk();                                       // Update marker values
    void PrepareSpgmImg(quint16 u32UpdateRows = 0 );        // Redraw the spectrogram to scale
    // Draw a scale for the Y-axis
    void DrawScaleDivY(QPainter *pPainter, qint16 i16Shift, qint16 i16DivSize);

protected slots:
    void Scale();                                           // Scaling
};

#endif // QSPGRAMM_H
