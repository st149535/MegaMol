///*  
// * IbvCommChannel.cpp
// *
// * Copyright (C) 2006 - 2011 by Visualisierungsinstitut Universitaet Stuttgart.
// * Alle Rechte vorbehalten.
// */
//
//#include "vislib/IbvCommChannel.h"
//
//#include "the/assert.h"
//#include "vislib/Exception.h"
//#include "vislib/IbvInformation.h"
//#include "vislib/IPCommEndPoint.h"
//#include "vislib/memutils.h"
//#include "vislib/MissingImplementationException.h"
//#include "vislib/sysfunctions.h"
//#include "the/trace.h"
//
//
///*
// * vislib::net::ib::IbvCommChannel::Accept
// */
//vislib::SmartRef<vislib::net::AbstractCommChannel> 
//vislib::net::ib::IbvCommChannel::Accept(void) {
//    VLSTACKTRACE("IbvCommChannel::Bind", __FILE__, __LINE__);
//
//    HRESULT hr = S_OK;
//
//    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "GetRequest.\n");
//    IWVConnectEndpoint *ep = NULL;
//    this->wvProvider->CreateConnectEndpoint(&ep);
//    if (FAILED(hr = this->connectEndPoint->GetRequest(ep, NULL))) {
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "GetRequest failed "
//            "with error code %d.\n", hr);
//        throw sys::COMException(hr, __FILE__, __LINE__);
//    }
//
//    WV_CONNECT_PARAM conParam;
//    ::ZeroMemory(&conParam, sizeof(conParam));
//
//    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "After GetRequest\n");
//
//    this->createQueuePair();
//
//    WV_QP_ATTRIBUTES qpAtts;
//    this->queuePair->Query(&qpAtts);
//    conParam.InitiatorDepth = 1;
//    //conParam.InitiatorDepth = qpAtts.InitiatorDepth;
//    //conParam.ResponderResources = qpAtts.ResponderResources;
//
//    if (FAILED(hr = this->connectEndPoint->Accept(this->queuePair, 
//            &conParam, NULL))) {
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Accepting client failed "
//            "with error code %d.\n", hr);
//        throw sys::COMException(hr, __FILE__, __LINE__);
//    }
//
//
//    return vislib::SmartRef<vislib::net::AbstractCommChannel>(NULL);
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::Bind
// */
//void vislib::net::ib::IbvCommChannel::Bind(
//        SmartRef<AbstractCommEndPoint> endPoint) {
//    VLSTACKTRACE("IbvCommChannel::Bind", __FILE__, __LINE__);
//    ////int result = 0;                     // RDMA CMA return values.
//
//    ////this->evtChannel = ::rdma_create_event_channel();
//    ////if (this->evtChannel == NULL) {
//    ////    throw Exception("TODO: Creation of RDMA event channel failed", __FILE__, __LINE__);
//    ////}
//
//    ////result = ::rdma_create_id(this->evtChannel, &this->id, this, RDMA_PS_TCP);
//    ////if (result != 0) {
//    ////    throw Exception("TODO: Allocation of RMDA communication identifiert failed", __FILE__, __LINE__);
//    ////}
//
//    //IPCommEndPoint *cep = endPoint.DynamicPeek<IPCommEndPoint>();
//    //IPEndPoint& ep = static_cast<IPEndPoint&>(*cep);
//    ////::rdma_bind_addr(this->id, static_cast<struct sockaddr *>(ep));
//    ////if (result != 0) {
//    ////    throw Exception("TODO: Binding server address failed", __FILE__, __LINE__);
//    ////}
//
//    //THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Creating IB end point...\n");
//    //if (FAILED(this->wvProvider->CreateConnectEndpoint(&this->connectEndPoint))) {
//    //    throw Exception("TODO: CreateConnectEndpoint failed", __FILE__, __LINE__);
//    //}
//
//    HRESULT hr = S_OK;
//    this->initialise();
//
//    IPCommEndPoint *cep = endPoint.DynamicPeek<IPCommEndPoint>();
//    IPEndPoint& ep = static_cast<IPEndPoint&>(*cep);
//
//    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Binding IB end point to %s...\n",
//        ep.ToStringA().PeekBuffer());
//    if (FAILED(hr = this->connectEndPoint->BindAddress(
//            static_cast<struct sockaddr *>(ep)))) {
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Binding IB end point failed "
//            "with error code %d.\n", hr);
//        throw sys::COMException(hr, __FILE__, __LINE__);
//    }
//
////    WV_CONNECT_ATTRIBUTES atts;
////    this->connectEndPoint->Query(&atts);
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::Close
// */
//void vislib::net::ib::IbvCommChannel::Close(void) {
//    VLSTACKTRACE("IbvCommChannel::Close", __FILE__, __LINE__);
//    // TODO
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::Connect
// */
//void vislib::net::ib::IbvCommChannel::Connect(
//        SmartRef<AbstractCommEndPoint> endPoint) {
//    VLSTACKTRACE("IbvCommChannel::Connect", __FILE__, __LINE__);
//    HRESULT hr = S_OK;
//
//    this->initialise();
//
//    IPCommEndPoint *cep = endPoint.DynamicPeek<IPCommEndPoint>();
//    IPEndPoint& ep = static_cast<IPEndPoint&>(*cep);
//
//    this->createQueuePair();
//    THE_ASSERT(this->queuePair != NULL);
//    THE_ASSERT(this->recvComplQueue != NULL);
//    THE_ASSERT(this->sendComplQueue != NULL);
//
//    WV_QP_ATTRIBUTES qpAttribs;
//    this->queuePair->Query(&qpAttribs);
//
//    WV_SGE sge;
//    sge.Length = 1;
//    sge.Lkey = this->recvKeys.Lkey;
//    sge.pAddress = this->recvRegion;
//
//    THE_ASSERT(this->queuePair != NULL);
//    hr = this->queuePair->PostReceive(NULL, &sge, 1);
//
//    WV_CONNECT_PARAM conParam;
//    ::ZeroMemory(&conParam, sizeof(conParam));
//    conParam.InitiatorDepth = 1;
//    conParam.ResponderResources = 1;
//    conParam.RetryCount = 7;
//    conParam.RnrRetryCount = 7;
//
//
//    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Connecting to IB end point "
//        "%s...\n", ep.ToStringA().PeekBuffer());
//    if (FAILED(hr = this->connectEndPoint->Connect(this->queuePair, 
//            static_cast<const sockaddr *>(ep), &conParam, NULL))) {
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Connection to IB end point "
//            "failed with error code %d.\n", hr);
//        throw sys::COMException(hr, __FILE__, __LINE__);        
//    }
//
//    // TODO
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::GetLocalEndPoint
// */
//vislib::SmartRef<vislib::net::AbstractCommEndPoint> 
//vislib::net::ib::IbvCommChannel::GetLocalEndPoint(void) const {
//    VLSTACKTRACE("IbvCommChannel::GetLocalEndPoint", __FILE__, __LINE__);
//    // TODO
//    throw vislib::MissingImplementationException("GetLocalEndPoint", __FILE__, __LINE__);
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::GetProvider
// */
//IWVProvider *vislib::net::ib::IbvCommChannel::GetProvider(void) {
//    VLSTACKTRACE("IbvCommChannel::GetProvider", __FILE__, __LINE__);
//    this->initialise();
//    THE_ASSERT(this->wvProvider != NULL);
//    return this->wvProvider;
//}
//
///*
// * vislib::net::ib::IbvCommChannel::GetRemoteEndPoint
// */
//vislib::SmartRef<vislib::net::AbstractCommEndPoint> 
//vislib::net::ib::IbvCommChannel::GetRemoteEndPoint(void) const {
//    VLSTACKTRACE("IbvCommChannel::GetRemoteEndPoint", __FILE__, __LINE__);
//    // TODO
//    throw vislib::MissingImplementationException("GetLocalEndPoint", __FILE__, __LINE__);
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::Listen
// */
//void vislib::net::ib::IbvCommChannel::Listen(const int backlog) {
//    VLSTACKTRACE("IbvCommChannel::Listen", __FILE__, __LINE__);
//    HRESULT hr = S_OK;
//
//    THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Setting IB end point into "
//        "listening state...\n");
//    if (FAILED(hr = this->connectEndPoint->Listen(backlog))) {
//        throw sys::COMException(hr, __FILE__, __LINE__);
//    }
//}
//
//
///* 
// * vislib::net::ib::IbvCommChannel::Receive
// */
//SIZE_T vislib::net::ib::IbvCommChannel::Receive(void *outData, 
//        const SIZE_T cntBytes, const UINT timeout,  const bool forceReceive) {
//    // TODO
//    return 0;
//}
//
//
///* 
// * vislib::net::ib::IbvCommChannel::Receive
// */
//SIZE_T vislib::net::ib::IbvCommChannel::Send(const void *data, 
//        const SIZE_T cntBytes, const UINT timeout, const bool forceSend) {
//    // TODO
//    return 0;
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::IbvCommChannel
// */
//vislib::net::ib::IbvCommChannel::IbvCommChannel(IWVProvider *wvProvider) 
//        : connectEndPoint(NULL), device(NULL), protectionDomain(NULL), 
//        queuePair(NULL), recvComplQueue(NULL), recvRegion(NULL), 
//        recvRegionSize(0), sendComplQueue(NULL), sendRegion(NULL),
//        sendRegionSize(0), wvProvider(wvProvider) {
//    VLSTACKTRACE("IbvCommChannel::IbvCommChannel", __FILE__, __LINE__);
//
//    ::ZeroMemory(&this->recvKeys, sizeof(this->recvKeys));
//    ::ZeroMemory(&this->sendKeys, sizeof(this->sendKeys));
//
//    if (this->wvProvider != NULL) {
//        this->wvProvider->AddRef();
//    }
//
//    // TODO:
//    this->recvRegionSize = 1024;
//    this->sendRegionSize = 1024;
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::~IbvCommChannel
// */
//vislib::net::ib::IbvCommChannel::~IbvCommChannel(void) {
//    VLSTACKTRACE("IbvCommChannel::~IbvCommChannel", __FILE__, __LINE__);
//
//    this->releaseQueuePair();
//    sys::SafeRelease(this->connectEndPoint);
//    sys::SafeRelease(this->device);
//    sys::SafeRelease(this->protectionDomain);
//    sys::SafeRelease(this->wvProvider);
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::createQueuePair
// */
//void vislib::net::ib::IbvCommChannel::createQueuePair(void) {
//    VLSTACKTRACE("IbvCommChannel::createQueuePair", __FILE__, __LINE__);
//    WV_CONNECT_ATTRIBUTES connAttribs;	// Receives attributes of end point.
//    WV_DEVICE_ATTRIBUTES devAttribs;    // Receives the device attributes.
//    HRESULT hr = S_OK;                  // Result of API calls.
//    WV_QP_CREATE qpCreate;              // Creation parameter for queue pair.
//
//    if (this->queuePair == NULL) {
//        ::ZeroMemory(&connAttribs, sizeof(connAttribs));
//        ::ZeroMemory(&devAttribs, sizeof(devAttribs));
//        ::ZeroMemory(&qpCreate, sizeof(qpCreate));
//
//        /* Get attributes of end point to get the device used for it. */
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Querying attributes of "
//            "connect end point...\n");
//        THE_ASSERT(this->connectEndPoint != NULL);
//        if (FAILED(hr = this->connectEndPoint->Query(&connAttribs))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Querying attributes of "
//                "connect end point failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//        // TODO: Evil; use first device if nothing found.
//        if (connAttribs.Device.DeviceGuid == 0) {
//            IbvInformation::DeviceList devices;
//            IbvInformation::GetInstance().GetDevices(devices);
//            connAttribs.Device.DeviceGuid = devices[0].GetNodeGuid();
//        }
//
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Opening IB device...\n");
//        THE_ASSERT(this->wvProvider != NULL);
//        THE_ASSERT(this->device == NULL);
//        if (FAILED(hr = this->wvProvider->OpenDevice(
//                connAttribs.Device.DeviceGuid, &this->device))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Opening IB device failed "
//                "with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Querying IB device "
//            "attributes...\n");
//        THE_ASSERT(this->device != NULL);
//        if (FAILED(hr = this->device->Query(&devAttribs))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Querying IB device "
//                "attributes failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//        /* Allocate a protection domain for the device. */
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Allocating protection "
//            "domain...\n");
//        THE_ASSERT(this->protectionDomain == NULL);
//        THE_ASSERT(this->device != NULL);
//        if (FAILED(hr = this->device->AllocateProtectionDomain(
//                &this->protectionDomain))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Allocating protection "
//                "domain failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//        /* Allocate and register memory buffers for sending and receiving. */
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Registering memory for "
//            "receiving data...\n");
//        THE_ASSERT(this->recvRegion == NULL);
//        THE_ASSERT(this->protectionDomain != NULL);
//        this->recvRegion = new BYTE[this->recvRegionSize];
//        if (FAILED(hr = this->protectionDomain->RegisterMemory(
//                this->recvRegion, this->recvRegionSize, 
//                IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE,
//                NULL, &this->recvKeys))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Registering memory for "
//                "receiving data failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Registering memory for "
//            "sending data...\n");
//        THE_ASSERT(this->sendRegion == NULL);
//        THE_ASSERT(this->protectionDomain != NULL);
//        this->sendRegion = new BYTE[this->sendRegionSize];
//        if (FAILED(hr = this->protectionDomain->RegisterMemory(
//                this->sendRegion, this->sendRegionSize, 
//                IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE,
//                NULL, &this->sendKeys))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Registering memory for "
//                "sending data failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//        /* Create completion queues. */
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Creating receive completion "
//            "queue...\n");
//        THE_ASSERT(this->recvComplQueue == NULL);
//        THE_ASSERT(this->device != NULL);
//        qpCreate.ReceiveDepth = 15;  // TODO
//        if (FAILED(hr = this->device->CreateCompletionQueue(
//                &qpCreate.ReceiveDepth, &this->recvComplQueue))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Creating receive "
//                "completion queue failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);        
//        }
//
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Creating send completion "
//            "queue...\n");
//        THE_ASSERT(this->sendComplQueue == NULL);
//        THE_ASSERT(this->device != NULL);
//        qpCreate.SendDepth = 15; // TODO
//        if (FAILED(hr = this->device->CreateCompletionQueue(
//                &qpCreate.SendDepth, &this->sendComplQueue))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Creating send "
//                "completion queue failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);        
//        }
//
//        /* Allocate the queue pair. */
//        qpCreate.pSendCq = this->sendComplQueue;
//        qpCreate.pReceiveCq = this->recvComplQueue;
//        //create.pSharedReceiveQueue = (qp_init_attr->srq != NULL) ?
//        //							 qp_init_attr->srq->handle : NULL;
//        qpCreate.Context = this;
//        //qpCreate.SendDepth = 10;
//        qpCreate.SendSge = 1;
//        //qpCreate.ReceiveDepth = 10;
//        qpCreate.ReceiveSge = 1;
//        qpCreate.MaxInlineSend = devAttribs.MaxInlineSend;
//        qpCreate.InitiatorDepth = 1;
//        qpCreate.ResponderResources = 0;
//        qpCreate.QpType = WvQpTypeRc;
//        //create.QpFlags = qp_init_attr->sq_sig_all ? WV_QP_SIGNAL_SENDS : 0;
//        
//
//        //qp_attr->send_cq = s_ctx->cq;
//        //qp_attr->recv_cq = s_ctx->cq;
//        //qp_attr->qp_type = IBV_QPT_RC;
//        //qp_attr->cap.max_send_wr = 10;
//        //qp_attr->cap.max_recv_wr = 10;
//        //qp_attr->cap.max_send_sge = 1;
//        //qp_attr->cap.max_recv_sge = 1;
//
//        //create.pSendCq = qp_init_attr->send_cq->handle;
//        //create.pReceiveCq = qp_init_attr->recv_cq->handle;
//        //create.pSharedReceiveQueue = (qp_init_attr->srq != NULL) ?
//        //							 qp_init_attr->srq->handle : NULL;
//        //create.Context = qp;
//        //create.SendDepth = qp_init_attr->cap.max_send_wr;
//        //create.SendSge = qp_init_attr->cap.max_send_sge;
//        //create.ReceiveDepth = qp_init_attr->cap.max_recv_wr;
//        //create.ReceiveSge = qp_init_attr->cap.max_recv_sge;
//        //create.MaxInlineSend = qp_init_attr->cap.max_inline_data;
//        //create.InitiatorDepth = 0;
//        //create.ResponderResources = 0;
//        //create.QpType = (WV_QP_TYPE) qp_init_attr->qp_type;
//        //create.QpFlags = qp_init_attr->sq_sig_all ? WV_QP_SIGNAL_SENDS : 0;
//
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Creating queue pair...\n");
//        THE_ASSERT(this->queuePair == NULL);
//        if (FAILED(hr = this->protectionDomain->CreateConnectQueuePair(
//            &qpCreate, &this->queuePair))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Creating queue pair failed "
//                "with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//
//    } /* end if (!this->isInitialised) */
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::initialise
// */
//void vislib::net::ib::IbvCommChannel::initialise(void) {
//    VLSTACKTRACE("IbvCommChannel::initialise", __FILE__, __LINE__);
//    HRESULT hr = S_OK;      // Result of API calls.
//
//    if (this->wvProvider == NULL) {
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Acquiring WinVerbs "
//            "provider...\n");
//        if (FAILED(hr = ::WvGetObject(IID_IWVProvider, 
//                reinterpret_cast<void **>(&this->wvProvider)))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Acquiring WinVerbs "
//                "provider failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);    
//        }
//    }
//
//    if (this->connectEndPoint == NULL) {
//        /* Create a connect end point. */
//        THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_INFO, "Creating IB connect end "
//            "point...\n");
//        if (FAILED(hr = this->wvProvider->CreateConnectEndpoint(
//                &this->connectEndPoint))) {
//            THE_TRACE(THE_TRCCHL_DEFAULT, THE_TRCLVL_ERROR, "Creating IB connect end "
//                "point failed with error code %d.\n", hr);
//            throw sys::COMException(hr, __FILE__, __LINE__);
//        }
//    } 
//
//    THE_ASSERT(this->wvProvider != NULL);
//    THE_ASSERT(this->connectEndPoint != NULL);
//}
//
//
///*
// * vislib::net::ib::IbvCommChannel::releaseQueuePair
// */
//void vislib::net::ib::IbvCommChannel::releaseQueuePair(void) {
//    VLSTACKTRACE("IbvCommChannel::releaseQueuePair", __FILE__, __LINE__);
//
//    sys::SafeRelease(this->queuePair);
//
//    if (this->recvKeys.Lkey != 0) {
//        THE_ASSERT(this->protectionDomain != NULL);
//        this->protectionDomain->DeregisterMemory(this->recvKeys.Lkey, NULL);
//    }
//    if (this->sendKeys.Lkey != 0) {
//        THE_ASSERT(this->protectionDomain != NULL);
//        this->protectionDomain->DeregisterMemory(this->sendKeys.Lkey, NULL);
//    }
//
//    ARY_SAFE_DELETE(this->recvRegion);
//    ARY_SAFE_DELETE(this->sendRegion);
//
//    sys::SafeRelease(this->recvComplQueue);
//    sys::SafeRelease(this->sendComplQueue);
//
//    THE_ASSERT(this->queuePair == NULL);
//    THE_ASSERT(this->recvRegion == NULL);
//    THE_ASSERT(this->recvComplQueue == NULL);
//    THE_ASSERT(this->sendRegion == NULL);
//    THE_ASSERT(this->sendComplQueue == NULL);
//}
