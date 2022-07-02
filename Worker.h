#pragma once

#include "JobQueue.h"

class Worker : public JobQueue
{

};

extern Worker* gWorker;