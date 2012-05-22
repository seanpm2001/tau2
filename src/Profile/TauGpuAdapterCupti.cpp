#include <Profile/TauGpuAdapterCupti.h>
#include <Profile/CuptiLayer.h>
#include <iostream>
using namespace std;

#if CUPTI_API_VERSION >= 2

void Tau_cupti_onload()
{
	//printf("in onload.\n");
	CUptiResult err;
	err = cuptiSubscribe(&subscriber, (CUpti_CallbackFunc)Tau_cupti_callback_dispatch, NULL);
  
	if (0 == strcasecmp(TauEnv_get_cupti_api(), "runtime") || 
			0 == strcasecmp(TauEnv_get_cupti_api(), "both"))
	if (cupti_api_runtime())
	{
		//printf("TAU: Subscribing to RUNTIME API.\n");
		err = cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RUNTIME_API);
		//runtime_enabled = true;
	}
  if (0 == strcasecmp(TauEnv_get_cupti_api(), "driver") || 
			0 == strcasecmp(TauEnv_get_cupti_api(), "both")) 
	if (cupti_api_driver())
	{
		//printf("TAU: Subscribing to DRIVER API.\n");
		err = cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_DRIVER_API);
		//driver_enabled = true;
	}

	err = cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_SYNCHRONIZE); 
	//err = cuptiEnableDomain(1, subscriber, CUPTI_CB_DOMAIN_RESOURCE); 

	CUDA_CHECK_ERROR(err, "Cannot set Domain.\n");

	//setup activity queue.
	activityBuffer = (uint8_t *)malloc(ACTIVITY_BUFFER_SIZE);
	err = cuptiActivityEnqueueBuffer(NULL, 0, activityBuffer, ACTIVITY_BUFFER_SIZE);
 	
	//to collect device info 
	err = cuptiActivityEnable(CUPTI_ACTIVITY_KIND_DEVICE);
	
	err = cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMCPY);
	err = cuptiActivityEnable(CUPTI_ACTIVITY_KIND_KERNEL);
	CUDA_CHECK_ERROR(err, "Cannot enqueue buffer.\n");

	Tau_gpu_init();
}

void Tau_cupti_onunload()
{
	//Tau_CuptiLayer_finalize();
	//printf("in onunload.\n");
	//if we have not yet registered any sync do so.
	//if we have registered a sync then Tau_profile_exit_all_threads will write
	//out the thread profiles already, do not attempt another sync.
	//cudaDeviceSynchronize();
  //CUptiResult err;
  //err = cuptiUnsubscribe(subscriber);
}

void Tau_cupti_callback_dispatch(void *ud, CUpti_CallbackDomain domain, CUpti_CallbackId id, const void *params)
{
  if (domain == CUPTI_CB_DOMAIN_RESOURCE && id == CUPTI_CBID_RESOURCE_CONTEXT_DESTROY_STARTING)
	{
		//printf("in callback domain = %d.\n", domain);
		//Tau_cupti_register_sync_event();
		/*
	  CUptiResult err;
		printf("in resource stream create callback.\n");
		CUpti_ResourceData* resource = (CUpti_ResourceData*) params;
		CUstream stream = (CUstream) resource->resourceHandle.stream;
		printf("in resource callback, stream retrieved.\n");
		uint32_t streamId;
		cuptiGetStreamId(resource->context, stream, &streamId);
		err = cuptiActivityEnqueueBuffer(resource->context, streamId, activityBuffer, ACTIVITY_BUFFER_SIZE);
	  CUDA_CHECK_ERROR(err, "Cannot enqueue buffer.\n");
		printf("in resource callback, enqueued buffer.\n");
		*/
	}
	else if (domain == CUPTI_CB_DOMAIN_SYNCHRONIZE)
	{
		//printf("register sync from callback.\n");
		Tau_cupti_register_sync_event();
	}
	else
	{
		const CUpti_CallbackData *cbInfo = (CUpti_CallbackData *) params;
		if (function_is_memcpy(id, domain))
		{
			int kind;
			int count;
			get_values_from_memcpy(cbInfo, id, domain, kind, count);
			if (cbInfo->callbackSite == CUPTI_API_ENTER)
			{
				FunctionInfo *p = TauInternal_CurrentProfiler(Tau_RtsLayer_getTid())->ThisFunction;
				functionInfoMap[cbInfo->correlationId] = p;	
				cuptiGpuId new_id = cuptiGpuId(cbInfo->contextUid, cbInfo->correlationId);
				Tau_gpu_enter_memcpy_event(
					cbInfo->functionName,
					&new_id,
					count,
					getMemcpyType(kind)
				);
			}
			else
			{
				Tau_gpu_exit_memcpy_event(
					cbInfo->functionName,
					&cuptiGpuId(cbInfo->contextUid, cbInfo->correlationId),
					getMemcpyType(kind)
				);
				if (function_is_sync(id))
				{
					//cerr << "sync function name: " << cbInfo->functionName << endl;
					//Disable counter tracking during the sync.
					Tau_CuptiLayer_disable();
					cuCtxSynchronize();
					Tau_CuptiLayer_enable();
					Tau_cupti_register_sync_event();
				}
			}
		}
		else
		{
			if (cbInfo->callbackSite == CUPTI_API_ENTER)
			{
				if (function_is_exit(id))
				{
					//Stop collecting cupti counters.
					Tau_CuptiLayer_finalize();
				}
				else if (function_is_launch(id))
				{
					FunctionInfo *p = TauInternal_CurrentProfiler(Tau_RtsLayer_getTid())->ThisFunction;
					functionInfoMap[cbInfo->correlationId] = p;	
					//printf("at launch id: %d.\n", cbInfo->correlationId);
					Tau_CuptiLayer_init();
				}
				Tau_gpu_enter_event(cbInfo->functionName);
			}
			else
			{
				Tau_gpu_exit_event(cbInfo->functionName);
				if (function_is_sync(id))
				{
					//cerr << "sync function name: " << cbInfo->functionName << endl;
					Tau_CuptiLayer_disable();
					cuCtxSynchronize();
					Tau_CuptiLayer_enable();
					Tau_cupti_register_sync_event();
				}
			}
		}
	}
}

void Tau_cupti_register_sync_event()
{
	//printf("in sync.\n");
	registered_sync = true;
  CUptiResult err, status;
  CUpti_Activity *record = NULL;
	size_t bufferSize = 0;

	err = cuptiActivityDequeueBuffer(NULL, 0, &activityBuffer, &bufferSize);
	//printf("activity buffer size: %d.\n", bufferSize);
	CUDA_CHECK_ERROR(err, "Cannot dequeue buffer.\n");
	
	do {
		status = cuptiActivityGetNextRecord(activityBuffer, bufferSize, &record);
		if (status == CUPTI_SUCCESS) {
			Tau_cupti_record_activity(record);
		}
		else if (status != CUPTI_ERROR_MAX_LIMIT_REACHED) {
	    CUDA_CHECK_ERROR(err, "Cannot get next record.\n");
			break;
		}	
	} while (status != CUPTI_ERROR_MAX_LIMIT_REACHED);
		
	size_t number_dropped;
	err = cuptiActivityGetNumDroppedRecords(NULL, 0, &number_dropped);

	if (number_dropped > 0)
		printf("TAU WARNING: %d CUDA records dropped, consider increasing the CUPTI_BUFFER size.", number_dropped);

	//requeue buffer
	err = cuptiActivityEnqueueBuffer(NULL, 0, activityBuffer, ACTIVITY_BUFFER_SIZE);
	CUDA_CHECK_ERROR(err, "Cannot requeue buffer.\n");
}

void Tau_cupti_record_activity(CUpti_Activity *record)
{
	//printf("in record activity");
  switch (record->kind) {
  	case CUPTI_ACTIVITY_KIND_MEMCPY:
		{	
      CUpti_ActivityMemcpy *memcpy = (CUpti_ActivityMemcpy *)record;
			//cerr << "recording memcpy: " << memcpy->end - memcpy->start << "ns.\n" << endl;
		  //cerr << "recording memcpy on device: " << memcpy->streamId << "/" << memcpy->runtimeCorrelationId << endl;
			int id;
			if (cupti_api_runtime())
			{
				id = memcpy->runtimeCorrelationId;
			}
			else
			{
				id = memcpy->correlationId;
			}
			cuptiGpuId gId = cuptiGpuId(memcpy->streamId, id);
			cuptiRecord cuRec = cuptiRecord(TAU_GPU_USE_DEFAULT_NAME, &gId, NULL); 
			Tau_gpu_register_memcpy_event(
				cuRec,
				memcpy->start / 1e3, 
				memcpy->end / 1e3, 
				TAU_GPU_UNKNOW_TRANSFER_SIZE, 
				getMemcpyType(memcpy->copyKind));
				
				break;
		}
  	case CUPTI_ACTIVITY_KIND_KERNEL:
		{
			//find FunctionInfo object from FunctionInfoMap
      CUpti_ActivityKernel *kernel = (CUpti_ActivityKernel *)record;
			//cerr << "recording kernel: " << kernel->name << ", " << kernel->end - kernel->start << "ns.\n" << endl;

			TauGpuContextMap map;
			static TauContextUserEvent* bs;
			static TauContextUserEvent* dm;
			static TauContextUserEvent* sm;
			static TauContextUserEvent* lm;
			static TauContextUserEvent* lr;
			Tau_get_context_userevent((void **) &bs, "Block Size");
			Tau_get_context_userevent((void **) &dm, "Shared Dynamic Memory (bytes)");
			Tau_get_context_userevent((void **) &sm, "Shared Static Memory (bytes)");
			Tau_get_context_userevent((void **) &lm, "Local Memory (bytes per thread)");
			Tau_get_context_userevent((void **) &lr, "Local Registers (per thread)");
			map[bs] = kernel->blockX * kernel->blockY * kernel->blockZ;
			map[dm] = kernel->dynamicSharedMemory;
			map[sm] = kernel->staticSharedMemory;
			map[lm] = kernel->localMemoryPerThread;
			map[lr] = kernel->registersPerThread;

			const char* name;
			int id;
			if (cupti_api_runtime())
			{
				id = kernel->runtimeCorrelationId;
			}
			else
			{
				id = kernel->correlationId;
				//printf("correlationid: %d.\n", id);
			}
			name = demangleName(kernel->name);
		  //cerr << "recording kernel on device: " << kernel->streamId << "/" << id << endl;
			cuptiGpuId gId = cuptiGpuId(kernel->streamId, id);
			cuptiRecord cuRec = cuptiRecord(name, &gId, &map);
			Tau_gpu_register_gpu_event(
				cuRec, 
				kernel->start / 1e3,
				kernel->end / 1e3);
				
				break;
		}
  	case CUPTI_ACTIVITY_KIND_DEVICE:
		{

			static bool recorded_metadata = false;
			if (!recorded_metadata)
			{

				CUpti_ActivityDevice *device = (CUpti_ActivityDevice *)record;
				
				//first the name.
				Tau_metadata("GPU Name", device->name);

				//the rest.
				RECORD_DEVICE_METADATA(computeCapabilityMajor, device);
				RECORD_DEVICE_METADATA(computeCapabilityMinor, device);
				RECORD_DEVICE_METADATA(constantMemorySize, device);
				RECORD_DEVICE_METADATA(coreClockRate, device);
				RECORD_DEVICE_METADATA(globalMemoryBandwidth, device);
				RECORD_DEVICE_METADATA(globalMemorySize, device);
				RECORD_DEVICE_METADATA(l2CacheSize, device);
				RECORD_DEVICE_METADATA(maxIPC, device);
				RECORD_DEVICE_METADATA(maxRegistersPerBlock, device);
				RECORD_DEVICE_METADATA(maxSharedMemoryPerBlock, device);
				RECORD_DEVICE_METADATA(maxThreadsPerBlock, device);
				RECORD_DEVICE_METADATA(maxWarpsPerMultiprocessor, device);
				RECORD_DEVICE_METADATA(numMemcpyEngines, device);
				RECORD_DEVICE_METADATA(numMultiprocessors, device);
				RECORD_DEVICE_METADATA(numThreadsPerWarp, device);
			
				recorded_metadata = true;
			}
			break;
		}
	}
}

bool function_is_sync(CUpti_CallbackId id)
{
	return (	
		//unstable results otherwise(
		//runtimeAPI
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaFree_v3021 ||
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaFreeArray_v3020 ||
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaFreeHost_v3020
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaEventRecord_v3020
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaThreadExit_v3020 || 
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020 ||
		id == CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020 ||
		id == CUPTI_RUNTIME_TRACE_CBID_cudaEventSynchronize_v3020 ||
		//id == CUPTI_RUNTIME_TRACE_CBID_cudaEventQuery_v3020 ||
		//driverAPI
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpy_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoD_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoH_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoD_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoA_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoA_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyAtoA_v2 ||
		id == CUPTI_DRIVER_TRACE_CBID_cuEventSynchronize //||
		//id == CUPTI_DRIVER_TRACE_CBID_cuEventQuery

				 );
}
bool function_is_exit(CUpti_CallbackId id)
{
	
	return (
		id == CUPTI_RUNTIME_TRACE_CBID_cudaThreadExit_v3020 || 
		id == CUPTI_RUNTIME_TRACE_CBID_cudaDeviceReset_v3020
		//driverAPI
				 );
	
}
bool function_is_launch(CUpti_CallbackId id) { 
	return id == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020 ||
		     id == CUPTI_DRIVER_TRACE_CBID_cuLaunchKernel;
}

bool function_is_memcpy(CUpti_CallbackId id, CUpti_CallbackDomain domain) {
	if (domain == CUPTI_CB_DOMAIN_RUNTIME_API)
	{
	return (
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpy_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArray_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArray_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyArrayToArray_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbol_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbol_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToArrayAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromArrayAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyToSymbolAsync_v3020 ||
		id ==     CUPTI_RUNTIME_TRACE_CBID_cudaMemcpyFromSymbolAsync_v3020
	);
	}
	else if (domain == CUPTI_CB_DOMAIN_DRIVER_API)
	{
		return (
		id ==     CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2 ||
		id ==     CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2
		);
	}
}

void get_values_from_memcpy(const CUpti_CallbackData *info, CUpti_CallbackId id, CUpti_CallbackDomain domain, int &kind, int &count)
{
	if (domain == CUPTI_CB_DOMAIN_RUNTIME_API)
	{
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpy, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyToArray, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyFromArray, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyArrayToArray, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyToSymbol, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyFromSymbol, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyAsync, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyToArrayAsync, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyFromArrayAsync, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyToSymbolAsync, id, info, kind, count)
    CAST_TO_RUNTIME_MEMCPY_TYPE_AND_CALL(cudaMemcpyFromSymbolAsync, id, info, kind, count)
	}
	//driver API
	else
	{
		if (id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyHtoD_v2)
		{
			kind = CUPTI_ACTIVITY_MEMCPY_KIND_HTOD;
			count = ((cuMemcpyHtoD_v2_params *) info->functionParams)->ByteCount;
		}
		else if (id == CUPTI_DRIVER_TRACE_CBID_cuMemcpyDtoH_v2)
		{
			kind = CUPTI_ACTIVITY_MEMCPY_KIND_DTOH;
			count = ((cuMemcpyDtoH_v2_params *) info->functionParams)->ByteCount;
		}
 
		else
		{
			//cannot find byte count
			kind = -1;
			count = 0;
		}

	}
}
int getMemcpyType(int kind)
{
	switch(kind)
	{
		case CUPTI_ACTIVITY_MEMCPY_KIND_HTOD:
			return MemcpyHtoD;
		case CUPTI_ACTIVITY_MEMCPY_KIND_HTOA:
			return MemcpyHtoD;
		case CUPTI_ACTIVITY_MEMCPY_KIND_DTOH:
			return MemcpyDtoH;
		case CUPTI_ACTIVITY_MEMCPY_KIND_ATOH:
			return MemcpyDtoH;
		case CUPTI_ACTIVITY_MEMCPY_KIND_ATOA:
			return MemcpyDtoD;
		case CUPTI_ACTIVITY_MEMCPY_KIND_ATOD:
			return MemcpyDtoD;
		case CUPTI_ACTIVITY_MEMCPY_KIND_DTOA:
			return MemcpyDtoD;
		case CUPTI_ACTIVITY_MEMCPY_KIND_DTOD:
			return MemcpyDtoD;
		default:
			return MemcpyUnknown;
	}
}

const char *demangleName(const char* name)
{
	const char *dem_name = 0;
	//printf("demangling: %s.\n", name);
#if defined(HAVE_GNU_DEMANGLE) && HAVE_GNU_DEMANGLE
	//printf("demangling name....\n");
	dem_name = cplus_demangle(name, DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE |
	DMGL_TYPES);
	//check to see if demangling failed (name was not mangled).
	if (dem_name == NULL)
	{
		dem_name = name;
	}
#else
	dem_name = name;
#endif /* HAVE_GPU_DEMANGLE */
	//printf("demanged: %s.\n", dem_name);
	return dem_name;
}


bool cupti_api_runtime()
{
	return (0 == strcasecmp(TauEnv_get_cupti_api(), "runtime") || 
			0 == strcasecmp(TauEnv_get_cupti_api(), "both"));
}
bool cupti_api_driver()
{
	return (0 == strcasecmp(TauEnv_get_cupti_api(), "driver") || 
			0 == strcasecmp(TauEnv_get_cupti_api(), "both")); 
}

#endif
