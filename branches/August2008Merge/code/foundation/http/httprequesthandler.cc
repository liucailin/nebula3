//------------------------------------------------------------------------------
//  httprequesthandler.cc
//  (C) 2007 Radon Labs GmbH
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "http/httprequesthandler.h"

namespace Http
{
ImplementClass(Http::HttpRequestHandler, 'HRHD', Core::RefCounted);

using namespace IO;
using namespace Util;

//------------------------------------------------------------------------------
/**
*/
HttpRequestHandler::HttpRequestHandler()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
HttpRequestHandler::~HttpRequestHandler()
{
    this->pendingRequests.SetSignalOnEnqueueEnabled(false);
}

//------------------------------------------------------------------------------
/**
    Put a http request into the request handlers message queue. This method
    is meant to be called from another thread.
*/
void
HttpRequestHandler::PutRequest(const Ptr<HttpRequest>& httpRequest)
{
    this->pendingRequests.Enqueue(httpRequest);
}

//------------------------------------------------------------------------------
/**
    Handle all pending http requests in the pending queue. This method
    must be called frequently from the thread which created this
    request handler.
*/
void
HttpRequestHandler::HandlePendingRequests()
{
    Array<Ptr<HttpRequest> > requests = this->pendingRequests.DequeueAll();
    IndexT i;
    for (i = 0; i < requests.Size(); i++)
    {
        this->HandleRequest(requests[i]);
        requests[i]->SetHandled(true);
    }
}

//------------------------------------------------------------------------------
/**
    Overwrite this method in your subclass. This method will be called by the
    HttpServer if AcceptsRequest() returned true. The request handler should
    properly process the request by filling the responseContentStream with
    data (for instance a HTML page), set the MediaType on the 
    responseContentStream (for instance "text/html") and return with a
    HttpStatus code (usually HttpStatus::OK).
*/
void
HttpRequestHandler::HandleRequest(const Ptr<HttpRequest>& request)
{
    request->SetStatus(HttpStatus::NotFound);
}

} // namespace Http
