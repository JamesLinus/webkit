/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "NamedNodeMap.h"

#include "Attr.h"
#include "Element.h"
#include "ExceptionCode.h"

namespace WebCore {

using namespace HTMLNames;

void NamedNodeMap::ref()
{
    m_element.ref();
}

void NamedNodeMap::deref()
{
    m_element.deref();
}

RefPtr<Attr> NamedNodeMap::getNamedItem(const AtomicString& name) const
{
    return m_element.getAttributeNode(name);
}

RefPtr<Attr> NamedNodeMap::getNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return m_element.getAttributeNodeNS(namespaceURI, localName);
}

RefPtr<Attr> NamedNodeMap::removeNamedItem(const AtomicString& name, ExceptionCode& ec)
{
    unsigned index = m_element.hasAttributes() ? m_element.findAttributeIndexByName(name, shouldIgnoreAttributeCase(m_element)) : ElementData::attributeNotFound;
    if (index == ElementData::attributeNotFound) {
        ec = NOT_FOUND_ERR;
        return nullptr;
    }
    return m_element.detachAttribute(index);
}

Vector<AtomicString> NamedNodeMap::supportedPropertyNames()
{
    // FIXME: Should be implemented.
    return Vector<AtomicString>();
}

RefPtr<Attr> NamedNodeMap::removeNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName, ExceptionCode& ec)
{
    unsigned index = m_element.hasAttributes() ? m_element.findAttributeIndexByName(QualifiedName(nullAtom, localName, namespaceURI)) : ElementData::attributeNotFound;
    if (index == ElementData::attributeNotFound) {
        ec = NOT_FOUND_ERR;
        return nullptr;
    }
    return m_element.detachAttribute(index);
}

RefPtr<Attr> NamedNodeMap::setNamedItem(Attr& attr, ExceptionCode& ec)
{
    return m_element.setAttributeNode(attr, ec);
}

RefPtr<Attr> NamedNodeMap::setNamedItem(Node& node, ExceptionCode& ec)
{
    if (!is<Attr>(node)) {
        ec = TypeError;
        return nullptr;
    }
    return setNamedItem(downcast<Attr>(node), ec);
}

RefPtr<Attr> NamedNodeMap::item(unsigned index) const
{
    if (index >= length())
        return 0;
    return m_element.ensureAttr(m_element.attributeAt(index).name());
}

unsigned NamedNodeMap::length() const
{
    if (!m_element.hasAttributes())
        return 0;
    return m_element.attributeCount();
}

} // namespace WebCore
