/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * https://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *  Includes
 ******************************************************************************/
#include "symboleditorstate_select.h"

#include "../dialogs/symbolpinpropertiesdialog.h"
#include "../symbolclipboarddata.h"
#include "../symboleditorwidget.h"
#include "cmd/cmdmoveselectedsymbolitems.h"
#include "cmd/cmdpastesymbolitems.h"
#include "cmd/cmdremoveselectedsymbolitems.h"
#include "cmd/cmdrotateselectedsymbolitems.h"

#include <librepcb/common/dialogs/circlepropertiesdialog.h>
#include <librepcb/common/dialogs/polygonpropertiesdialog.h>
#include <librepcb/common/dialogs/textpropertiesdialog.h>
#include <librepcb/common/graphics/circlegraphicsitem.h>
#include <librepcb/common/graphics/graphicsscene.h>
#include <librepcb/common/graphics/graphicsview.h>
#include <librepcb/common/graphics/polygongraphicsitem.h>
#include <librepcb/common/graphics/textgraphicsitem.h>
#include <librepcb/common/undostack.h>
#include <librepcb/library/sym/symbol.h>
#include <librepcb/library/sym/symbolgraphicsitem.h>
#include <librepcb/library/sym/symbolpingraphicsitem.h>

#include <QtCore>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace library {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

SymbolEditorState_Select::SymbolEditorState_Select(
    const Context& context) noexcept
  : SymbolEditorState(context), mState(SubState::IDLE) {
}

SymbolEditorState_Select::~SymbolEditorState_Select() noexcept {
  Q_ASSERT(mCmdMoveSelectedItems.isNull());
}

/*******************************************************************************
 *  Event Handlers
 ******************************************************************************/

bool SymbolEditorState_Select::processGraphicsSceneMouseMoved(
    QGraphicsSceneMouseEvent& e) noexcept {
  Point startPos   = Point::fromPx(e.buttonDownScenePos(Qt::LeftButton));
  Point currentPos = Point::fromPx(e.scenePos());

  switch (mState) {
    case SubState::SELECTING: {
      setSelectionRect(startPos, currentPos);
      return true;
    }
    case SubState::MOVING: {
      if (!mCmdMoveSelectedItems) {
        mCmdMoveSelectedItems.reset(
            new CmdMoveSelectedSymbolItems(mContext, startPos));
      }
      mCmdMoveSelectedItems->setCurrentPosition(currentPos);
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processGraphicsSceneLeftMouseButtonPressed(
    QGraphicsSceneMouseEvent& e) noexcept {
  Point pos = Point::fromPx(e.scenePos());

  switch (mState) {
    case SubState::IDLE: {
      // get items under cursor
      QList<QSharedPointer<SymbolPinGraphicsItem>> pins;
      QList<QSharedPointer<CircleGraphicsItem>>    circles;
      QList<QSharedPointer<PolygonGraphicsItem>>   polygons;
      QList<QSharedPointer<TextGraphicsItem>>      texts;
      int count = mContext.symbolGraphicsItem.getItemsAtPosition(
          pos, &pins, &circles, &polygons, &texts);
      if (count == 0) {
        // start selecting
        clearSelectionRect(true);
        mState = SubState::SELECTING;
      } else {
        // check if the top most item under the cursor is already selected
        QGraphicsItem* topMostItem = nullptr;
        if (pins.count() > 0) {
          topMostItem = pins.first().data();
        } else if (texts.count() > 0) {
          topMostItem = texts.first().data();
        } else if (polygons.count() > 0) {
          topMostItem = polygons.first().data();
        } else if (circles.count() > 0) {
          topMostItem = circles.first().data();
        } else {
          Q_ASSERT(false);
        }
        bool itemAlreadySelected = topMostItem->isSelected();

        if (e.modifiers().testFlag(Qt::ControlModifier)) {
          // Toggle selection when CTRL is pressed
          if (dynamic_cast<SymbolPinGraphicsItem*>(topMostItem)) {
            // workaround for selection of a SymbolPinGraphicsItem
            dynamic_cast<SymbolPinGraphicsItem*>(topMostItem)
                ->setSelected(!itemAlreadySelected);
          } else {
            topMostItem->setSelected(!itemAlreadySelected);
          }
        } else if (!itemAlreadySelected) {
          // Only select the topmost item when clicking an unselected item
          // without CTRL
          clearSelectionRect(true);
          if (dynamic_cast<SymbolPinGraphicsItem*>(topMostItem)) {
            // workaround for selection of a SymbolPinGraphicsItem
            dynamic_cast<SymbolPinGraphicsItem*>(topMostItem)
                ->setSelected(true);
          } else {
            topMostItem->setSelected(true);
          }
        }

        // start moving
        Q_ASSERT(!mCmdMoveSelectedItems);
        mState = SubState::MOVING;
      }
      return true;
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processGraphicsSceneLeftMouseButtonReleased(
    QGraphicsSceneMouseEvent& e) noexcept {
  Q_UNUSED(e);
  switch (mState) {
    case SubState::SELECTING: {
      clearSelectionRect(false);
      mState = SubState::IDLE;
      return true;
    }
    case SubState::MOVING: {
      if (mCmdMoveSelectedItems) {
        try {
          mContext.undoStack.execCmd(mCmdMoveSelectedItems.take());
        } catch (const Exception& e) {
          QMessageBox::critical(&mContext.editorWidget, tr("Error"),
                                e.getMsg());
        }
      }
      mState = SubState::IDLE;
      return true;
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processGraphicsSceneLeftMouseButtonDoubleClicked(
    QGraphicsSceneMouseEvent& e) noexcept {
#if (QT_VERSION < QT_VERSION_CHECK(5, 3, 0))
  // abort moving and handle double click
  if (mState == SubState::MOVING) {
    mCmdMoveSelectedItems.reset();
    mState = SubState::IDLE;
  }
#endif

  if (mState == SubState::IDLE) {
    return openPropertiesDialogOfItemAtPos(Point::fromPx(e.scenePos()));
  } else {
    return false;
  }
}

bool SymbolEditorState_Select::processGraphicsSceneRightMouseButtonReleased(
    QGraphicsSceneMouseEvent& e) noexcept {
  if (mState == SubState::IDLE) {
    return openContextMenuAtPos(Point::fromPx(e.scenePos()));
  } else {
    return false;
  }
}

bool SymbolEditorState_Select::processCut() noexcept {
  switch (mState) {
    case SubState::IDLE: {
      if (copySelectedItemsToClipboard()) {
        return removeSelectedItems();
      } else {
        return false;
      }
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processCopy() noexcept {
  switch (mState) {
    case SubState::IDLE: {
      return copySelectedItemsToClipboard();
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processPaste() noexcept {
  switch (mState) {
    case SubState::IDLE: {
      return pasteFromClipboard();
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processRotateCw() noexcept {
  switch (mState) {
    case SubState::IDLE: {
      return rotateSelectedItems(-Angle::deg90());
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processRotateCcw() noexcept {
  switch (mState) {
    case SubState::IDLE: {
      return rotateSelectedItems(Angle::deg90());
    }
    default: { return false; }
  }
}

bool SymbolEditorState_Select::processRemove() noexcept {
  switch (mState) {
    case SubState::IDLE: {
      return removeSelectedItems();
    }
    default: { return false; }
  }
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

bool SymbolEditorState_Select::openContextMenuAtPos(const Point& pos) noexcept {
  // build the context menu
  QMenu    menu;
  QAction* aRotateCCW =
      menu.addAction(QIcon(":/img/actions/rotate_left.png"), tr("Rotate"));
  QAction* aRemove =
      menu.addAction(QIcon(":/img/actions/delete.png"), tr("Remove"));
  menu.addSeparator();
  QAction* aProperties = menu.addAction(tr("Properties"));

  // execute the context menu
  QAction* action = menu.exec(QCursor::pos());
  if (action == aRotateCCW) {
    return rotateSelectedItems(Angle::deg90());
  } else if (action == aRemove) {
    return removeSelectedItems();
  } else if (action == aProperties) {
    return openPropertiesDialogOfItemAtPos(pos);
  } else {
    return false;
  }
}

bool SymbolEditorState_Select::openPropertiesDialogOfItemAtPos(
    const Point& pos) noexcept {
  QList<QSharedPointer<SymbolPinGraphicsItem>> pins;
  QList<QSharedPointer<CircleGraphicsItem>>    circles;
  QList<QSharedPointer<PolygonGraphicsItem>>   polygons;
  QList<QSharedPointer<TextGraphicsItem>>      texts;
  mContext.symbolGraphicsItem.getItemsAtPosition(pos, &pins, &circles,
                                                 &polygons, &texts);

  if (pins.count() > 0) {
    SymbolPinGraphicsItem* item =
        dynamic_cast<SymbolPinGraphicsItem*>(pins.first().data());
    Q_ASSERT(item);
    SymbolPinPropertiesDialog dialog(item->getPin(), mContext.undoStack,
                                     &mContext.editorWidget);
    dialog.exec();
    return true;
  } else if (texts.count() > 0) {
    TextGraphicsItem* item =
        dynamic_cast<TextGraphicsItem*>(texts.first().data());
    Q_ASSERT(item);
    TextPropertiesDialog dialog(
        item->getText(), mContext.undoStack,
        mContext.layerProvider.getSchematicGeometryElementLayers(),
        &mContext.editorWidget);
    dialog.exec();
    return true;
  } else if (polygons.count() > 0) {
    PolygonGraphicsItem* item =
        dynamic_cast<PolygonGraphicsItem*>(polygons.first().data());
    Q_ASSERT(item);
    PolygonPropertiesDialog dialog(
        item->getPolygon(), mContext.undoStack,
        mContext.layerProvider.getSchematicGeometryElementLayers(),
        &mContext.editorWidget);
    dialog.exec();
    return true;
  } else if (circles.count() > 0) {
    CircleGraphicsItem* item =
        dynamic_cast<CircleGraphicsItem*>(circles.first().data());
    Q_ASSERT(item);
    CirclePropertiesDialog dialog(
        item->getCircle(), mContext.undoStack,
        mContext.layerProvider.getSchematicGeometryElementLayers(),
        &mContext.editorWidget);
    dialog.exec();
    return true;
  } else {
    return false;
  }
}

bool SymbolEditorState_Select::copySelectedItemsToClipboard() noexcept {
  try {
    SymbolClipboardData data(mContext.symbol.getUuid());
    // pins
    foreach (const QSharedPointer<SymbolPinGraphicsItem>& pin,
             mContext.symbolGraphicsItem.getSelectedPins()) {
      Q_ASSERT(pin);
      data.getPins().append(std::make_shared<SymbolPin>(pin->getPin()));
    }
    // circles
    foreach (const QSharedPointer<CircleGraphicsItem>& circle,
             mContext.symbolGraphicsItem.getSelectedCircles()) {
      Q_ASSERT(circle);
      data.getCircles().append(std::make_shared<Circle>(circle->getCircle()));
    }
    // polygons
    foreach (const QSharedPointer<PolygonGraphicsItem>& polygon,
             mContext.symbolGraphicsItem.getSelectedPolygons()) {
      Q_ASSERT(polygon);
      data.getPolygons().append(
          std::make_shared<Polygon>(polygon->getPolygon()));
    }
    // texts
    foreach (const QSharedPointer<TextGraphicsItem>& text,
             mContext.symbolGraphicsItem.getSelectedTexts()) {
      Q_ASSERT(text);
      data.getTexts().append(std::make_shared<Text>(text->getText()));
    }
    // copy to clipboard
    qApp->clipboard()->setMimeData(data.toMimeData().release());
  } catch (const Exception& e) {
    QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
  }
  return true;
}

bool SymbolEditorState_Select::pasteFromClipboard() noexcept {
  try {
    std::unique_ptr<SymbolClipboardData> data =
        SymbolClipboardData::fromMimeData(
            qApp->clipboard()->mimeData());  // can throw
    if (data) {
      Point posOffset = mContext.graphicsView.mapGlobalPosToScenePos(
          QCursor::pos(), true, true);
      mContext.undoStack.execCmd(
          new CmdPasteSymbolItems(mContext.symbol, std::move(data), posOffset));
    }
  } catch (const Exception& e) {
    QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
  }
  return true;
}

bool SymbolEditorState_Select::rotateSelectedItems(
    const Angle& angle) noexcept {
  try {
    mContext.undoStack.execCmd(
        new CmdRotateSelectedSymbolItems(mContext, angle));
  } catch (const Exception& e) {
    QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
  }
  return true;  // TODO: return false if no items were selected
}

bool SymbolEditorState_Select::removeSelectedItems() noexcept {
  try {
    mContext.undoStack.execCmd(new CmdRemoveSelectedSymbolItems(mContext));
  } catch (const Exception& e) {
    QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
  }
  return true;  // TODO: return false if no items were selected
}

void SymbolEditorState_Select::setSelectionRect(const Point& p1,
                                                const Point& p2) noexcept {
  mContext.graphicsScene.setSelectionRect(p1, p2);
  mContext.symbolGraphicsItem.setSelectionRect(
      QRectF(p1.toPxQPointF(), p2.toPxQPointF()));
}

void SymbolEditorState_Select::clearSelectionRect(
    bool updateItemsSelectionState) noexcept {
  mContext.graphicsScene.setSelectionRect(Point(), Point());
  if (updateItemsSelectionState) {
    mContext.graphicsScene.setSelectionArea(QPainterPath());
  }
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace library
}  // namespace librepcb
