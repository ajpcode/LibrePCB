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
#include "symbolclipboarddata.h"

#include <QtCore>
#include <QtWidgets>

/*******************************************************************************
 *  Namespace
 ******************************************************************************/
namespace librepcb {
namespace library {
namespace editor {

/*******************************************************************************
 *  Constructors / Destructor
 ******************************************************************************/

SymbolClipboardData::SymbolClipboardData() noexcept {
}

SymbolClipboardData::SymbolClipboardData(const SExpression& node)
  : mPins(node), mPolygons(node), mCircles(node), mTexts(node) {
}

SymbolClipboardData::~SymbolClipboardData() noexcept {
}

/*******************************************************************************
 *  General Methods
 ******************************************************************************/

std::unique_ptr<QMimeData> SymbolClipboardData::toMimeData() const {
  SExpression root =
      serializeToDomElement("librepcb_clipboard_symbol");  // can throw
  QByteArray content = root.toByteArray();

  std::unique_ptr<QMimeData> data(new QMimeData());
  data->setData("application/x-librepcb-symbol", content);
  data->setText(content);
  return std::move(data);
}

/*******************************************************************************
 *  Private Methods
 ******************************************************************************/

void SymbolClipboardData::serialize(SExpression& root) const {
  mPins.serialize(root);
  mPolygons.serialize(root);
  mCircles.serialize(root);
  mTexts.serialize(root);
}

/*******************************************************************************
 *  End of File
 ******************************************************************************/

}  // namespace editor
}  // namespace library
}  // namespace librepcb
