#ifndef QCENTRALWIDGET_H
#define QCENTRALWIDGET_H

#include <QWidget>
#include "cchartprm.h"
#include "spgramm.h"

#define DEF_FFT_SIZE 16384

class CCentralWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CCentralWidget(QWidget *parent = 0);
    ~CCentralWidget();

public slots:
    void onUpdatePrms(quint8 ui8Mode);          // Updating parameters
    void onStart(quint8 ui8Mode);               // Start/Finish adding data

protected:
    CSpgramm        *m_pSpgmWgt;
    CChartPrm       *m_pChartPrm;
    int             m_nTimer;
    bool            m_bRead4File;
    quint64         m_u64Calc, m_u64Update;

    void timerEvent(QTimerEvent *event);

protected slots:
    void onScale();
    void onMrk1( bool bChecked );
    void onMrk2( bool bChecked );
};

#endif // QCENTRALWIDGET_H
