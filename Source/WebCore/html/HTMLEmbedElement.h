/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLEmbedElement_h
#define HTMLEmbedElement_h

#include "HTMLPlugInImageElement.h"

namespace WebCore {

class HTMLEmbedElement final : public HTMLPlugInImageElement {
public:
    static Ref<HTMLEmbedElement> create(const QualifiedName&, Document&, bool createdByParser);

private:
    HTMLEmbedElement(const QualifiedName&, Document&, bool createdByParser);

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    bool isPresentationAttribute(const QualifiedName&) const final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) final;

    bool rendererIsNeeded(const RenderStyle&) final;

    bool isURLAttribute(const Attribute&) const final;
    const AtomicString& imageSourceURL() const final;

    RenderWidget* renderWidgetLoadingPlugin() const final;

    void updateWidget(PluginCreationOption) final;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    void parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues);
};

}

#endif
