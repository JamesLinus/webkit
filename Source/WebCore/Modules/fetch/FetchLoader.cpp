/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchLoader.h"

#if ENABLE(FETCH_API)

#include "BlobURL.h"
#include "FetchBody.h"
#include "FetchLoaderClient.h"
#include "FetchRequest.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "ThreadableBlobRegistry.h"
#include "ThreadableLoader.h"

namespace WebCore {

void FetchLoader::start(ScriptExecutionContext& context, Blob& blob)
{
    auto urlForReading = BlobURL::createPublicURL(context.securityOrigin());
    if (urlForReading.isEmpty()) {
        m_client.didFail();
        return;
    }

    ThreadableBlobRegistry::registerBlobURL(context.securityOrigin(), urlForReading, blob.url());

    ResourceRequest request(urlForReading);
    request.setHTTPMethod("GET");

    ThreadableLoaderOptions options;
    options.setSendLoadCallbacks(SendCallbacks);
    options.setSniffContent(DoNotSniffContent);
    options.setDataBufferingPolicy(DoNotBufferData);
    options.preflightPolicy = ConsiderPreflight;
    options.setAllowCredentials(AllowStoredCredentials);
    options.mode = FetchOptions::Mode::SameOrigin;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    m_loader = ThreadableLoader::create(&context, this, request, options);
    m_isStarted = m_loader;
}

void FetchLoader::start(ScriptExecutionContext& context, const FetchRequest& request)
{
    ThreadableLoaderOptions options;
    options.setSendLoadCallbacks(SendCallbacks);
    options.setSniffContent(DoNotSniffContent);
    options.setDataBufferingPolicy(DoNotBufferData);
    options.preflightPolicy = ConsiderPreflight;
    options.setAllowCredentials(AllowStoredCredentials);
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    // FIXME: Pass directly all fetch options to loader options.
    options.redirect = request.fetchOptions().redirect;
    options.mode = request.fetchOptions().mode;
    if (options.mode == FetchOptions::Mode::Cors)
        options.setAllowCredentials(DoNotAllowStoredCredentials);

    m_loader = ThreadableLoader::create(&context, this, request.internalRequest(), options);
    m_isStarted = m_loader;
}

FetchLoader::FetchLoader(FetchLoaderClient& client, FetchBodyConsumer* consumer)
    : m_client(client)
    , m_consumer(consumer)
{
}

void FetchLoader::stop()
{
    if (m_consumer)
        m_consumer->clean();
    if (m_loader)
        m_loader->cancel();
}

RefPtr<SharedBuffer> FetchLoader::startStreaming()
{
    ASSERT(m_consumer);
    auto firstChunk = m_consumer->takeData();
    m_consumer = nullptr;
    return firstChunk;
}

void FetchLoader::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    m_client.didReceiveResponse(response);
}

void FetchLoader::didReceiveData(const char* value, int size)
{
    if (!m_consumer) {
        m_client.didReceiveData(value, size);
        return;
    }
    m_consumer->append(value, size);
}

void FetchLoader::didFinishLoading(unsigned long, double)
{
    m_client.didSucceed();
}

void FetchLoader::didFail(const ResourceError&)
{
    m_client.didFail();
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
