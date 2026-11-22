/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/11/2026 by @author Tsukini

File Name:
##  @file LayoutView.hpp

File Description:
##  Editor of the layout (drag the devices, snapped edges) and live map of every cursor
\**************************************************************/

#ifndef LAYOUTVIEW_H
    #define LAYOUTVIEW_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _hot, _nodiscard
    #include "../../core/Hub.hpp"       // xmouse::core::Hub
    #include <QWidget>                  // QWidget
    #include <QPointF>                  // QPointF
    #include <QString>                  // QString
    #include <QPoint>                   // QPoint

namespace xmouse::ui::gui { // namespace start

//----------------------------------------------------------------//
/* CLASS */

class LayoutView final: public QWidget {
    Q_OBJECT

    private:
        core::Hub& _hub;
        QString _dragged = ""; // device being moved
        QPoint _dragStart; // widget pixels
        QPoint _dragOrigin; // global position of the device when the drag started
        QPoint _dragPosition; // snapped position shown while dragging
        QRect _view; // global space shown (computed at each paint)
        double _scale = 0.1; // widget pixels per global pixel
        QPointF _offset; // widget position of the global (0, 0)
        bool _editable = true;

        // ---------- Pre-Function -------- //
        void fit_(void); // _view, _scale, _offset from the layout
        _hot _nodiscard QString deviceAt_(const QPoint& widget) const;

        // ------------ Function ---------- //
        /* tools */
        _hot _nodiscard inline QPointF toWidget_(const QPoint& global) const {return QPointF(global.x() * this->_scale, global.y() * this->_scale) + this->_offset;};

    protected:
        // ---------- Pre-Function -------- //
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

    public:
        // ------------ Function ---------- //
        /* setter */
        _cold inline void setEditable(const bool editable) {this->_editable = editable;};

        /* getter */
        _cold _nodiscard QSize sizeHint(void) const override        {return QSize(720, 360);};
        _cold _nodiscard QSize minimumSizeHint(void) const override {return QSize(320, 200);};

        // ---------- Constructor --------- //
        explicit LayoutView(core::Hub& hub, QWidget* parent = nullptr);

        // ----------- Destructor --------- //
        ~LayoutView() override = default;
};

} // namespace end
#endif /* LAYOUTVIEW_H */
