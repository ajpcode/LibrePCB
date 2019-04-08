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
#include "cmdpastesymbolitems.h"

#include "../../symbolclipboarddata.h"

#include <librepcb/common/toolbox.h>
#include <librepcb/library/sym/symbol.h>

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

CmdPasteSymbolItems::CmdPasteSymbolItems(
    Symbol& symbol, std::unique_ptr<SymbolClipboardData> data,
    const Point& positionOffset) noexcept
  : UndoCommandGroup(tr("Paste Symbol Elements")),
    mSymbol(symbol),
    mData(std::move(data)),
    mPositionOffset(positionOffset) {
  Q_ASSERT(mData);
}

CmdPasteSymbolItems::~CmdPasteSymbolItems() noexcept {
}

/*******************************************************************************
 *  Inherited from UndoCommand
 ******************************************************************************/

bool CmdPasteSymbolItems::performExecute() {
  // Notes:
  //
  //  - If the UUID is already existing, or the destination symbol is different
  //    to the source symbol, generate a new random UUID. Otherwise use the same
  //    UUID to avoid modifications after cut+paste within one symbol.
  //  - If there is already a pin with the same name, increment its number (or
  //    start adding a number if there is none already) to get unique names.

  for (const SymbolPin& pin : mData->getPins()) {
    Uuid uuid = pin.getUuid();
    if (mSymbol.getPins().contains(uuid) ||
        (mSymbol.getUuid() != mData->getSymbolUuid())) {
      uuid = Uuid::createRandom();
    }
    CircuitIdentifier name = pin.getName();
    for (int i = 0; (i < 1000) && mSymbol.getPins().contains(*name); ++i) {
      name = CircuitIdentifier(
          Toolbox::incrementNumberInString(*name));  // can throw
    }
    std::shared_ptr<SymbolPin> copy = std::make_shared<SymbolPin>(
        uuid, name, pin.getPosition() + mPositionOffset, pin.getLength(),
        pin.getRotation());
    appendChild(new CmdSymbolPinInsert(mSymbol.getPins(), copy));
  }

  for (const Circle& circle : mData->getCircles()) {
    Uuid uuid = circle.getUuid();
    if (mSymbol.getCircles().contains(uuid) ||
        (mSymbol.getUuid() != mData->getSymbolUuid())) {
      uuid = Uuid::createRandom();
    }
    std::shared_ptr<Circle> copy = std::make_shared<Circle>(
        uuid, circle.getLayerName(), circle.getLineWidth(), circle.isFilled(),
        circle.isGrabArea(), circle.getCenter() + mPositionOffset,
        circle.getDiameter());
    appendChild(new CmdCircleInsert(mSymbol.getCircles(), copy));
  }

  for (const Polygon& polygon : mData->getPolygons()) {
    Uuid uuid = polygon.getUuid();
    if (mSymbol.getPolygons().contains(uuid) ||
        (mSymbol.getUuid() != mData->getSymbolUuid())) {
      uuid = Uuid::createRandom();
    }
    std::shared_ptr<Polygon> copy = std::make_shared<Polygon>(
        uuid, polygon.getLayerName(), polygon.getLineWidth(),
        polygon.isFilled(), polygon.isGrabArea(),
        polygon.getPath().translated(mPositionOffset));
    appendChild(new CmdPolygonInsert(mSymbol.getPolygons(), copy));
  }

  for (const Text& text : mData->getTexts()) {
    Uuid uuid = text.getUuid();
    if (mSymbol.getTexts().contains(uuid) ||
        (mSymbol.getUuid() != mData->getSymbolUuid())) {
      uuid = Uuid::createRandom();
    }
    std::shared_ptr<Text> copy = std::make_shared<Text>(
        uuid, text.getLayerName(), text.getText(),
        text.getPosition() + mPositionOffset, text.getRotation(),
        text.getHeight(), text.getAlign());
    appendChild(new CmdTextInsert(mSymbol.getTexts(), copy));
  }

  // execute all child commands
  return UndoCommandGroup::performExecute();  // can throw
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace library
}  // namespace librepcb
