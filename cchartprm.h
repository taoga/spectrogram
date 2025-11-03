#ifndef QCHARTPRM_H
#define QCHARTPRM_H

#include <QDialog>

namespace Ui {
class CChartPrm;
}

#define DEF_FFT_SIZE    16384

class CChartPrm : public QDialog
{
    Q_OBJECT

public:
    explicit CChartPrm(QWidget *parent = 0);
    ~CChartPrm();
    // Axe X
    QString GetTitleX();
    QString GetUnitX();
    double GetMinX();
    double GetMaxX();
    quint16 GetPrecX();
    // Axe Y
    QString GetTitleY();
    QString GetUnitY();
    double GetMinY();
    double GetMaxY();
    quint16 GetPrecY();
    // Axe Z
    QString GetTitleZ();
    QString GetUnitZ();
    double GetMinZ();
    double GetMaxZ();
    quint16 GetPrecZ();

    QString GetTitleSpgm();
    quint64 GetFftSize();
    bool    GetDrawTitle();

    bool GetShiftUp2Down();
    void SetMrkChecked(bool bCheck);

signals:
    void sigUpdate( quint8 ui8Mode );
    void sigStart( quint8 ui8Mode );
    void Scale();
    void Mrk1( bool bChecked );
    void Mrk2( bool bChecked );

private slots:
    void on_pbUpdate_clicked();
    void on_pbStart_clicked();
    void on_tbtnScale_clicked();
    void on_tbtnMrk1_clicked();
    void on_tbtnMrk2_clicked();
    void onUpdatedMrk1( double lfX, double lfY );
    void onUpdatedMrk2( double lfX, double lfY );

private:
    Ui::CChartPrm *ui;
    quint8        m_u8Start;
    double        m_lfMrk1X, m_lfMrk1Y;
    double        m_lfMrk2X, m_lfMrk2Y;
};

#endif // QCHARTPRM_H
