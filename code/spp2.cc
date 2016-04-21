#include <systemc.h>
#include <hapi.h>
#include <processor.h>
#include <sensortask.h>
#include <processortask.h>
#include <defaultprocesstypes.h>

using namespace std;

sc_trace_file *tfHapi;

class top : public ProcessNetwork {
  public:
    top(sc_module_name name) : ProcessNetwork(name) {

      HAPI::TimingInformation fdsTask0(
        30,      
        30,      
        SC_NS,
        1        
      );

      HAPI::TimingInformation fdsTask1(
        30,      
        30,      
        SC_NS,   
        1        
      );

      int bufferSize0 = 1;
      CircularBuffer<void*> *fifo0= new CircularBuffer<void*>("Fifo0", 
                                                                bufferSize0, 1, 1);
      int bufferSize1 = 1;
      CircularBuffer<void*> *fifo1= new CircularBuffer<void*>("Fifo1", 
                                                                bufferSize1, 1, 1);
      double src0Period = 50;
      double src0FiringDuration = 0;
      DefaultSensorTask* src0 = new DefaultSensorTask("source0", tfHapi, 
                                                        src0FiringDuration, SC_NS);
      src0->setPeriod(sc_time(src0Period, SC_NS));
      src00->setPeriod(sc_time(src0Period, SC_NS));


      double src1Period = 100;
      double src1FiringDuration = 0;
      double src1setInitialDelay = 30;
      DefaultSensorTask* src1 = new DefaultSensorTask("source1", tfHapi, 
                                                        src1FiringDuration, SC_NS);
      src1->setPeriod(sc_time(src1Period, SC_NS));
      src1->setInitialDelay(sc_time(src1setInitialDelay, SC_NS));

      int priority0 = 1;
      int priority1 = 2;
      Processor* p = new Processor("proc", Processor::SCHED_FIXED_PRIORITY_PREEMPTIVE, 
                                                        tfHapi);
      DefaultProcessorTask* t0 = new DefaultProcessorTask("task0", tfHapi,    
                                                            fdsTask0, priority0, 0);
      DefaultProcessorTask* t1 = new DefaultProcessorTask("task1", tfHapi, 
                                                            fdsTask1, priority1, 0);

      t0->enablePrintResponseTime();
      t1->enablePrintResponseTime();

      p->addTask(t0);
      p->addTask(t1);

      src0->addOutputPort()->bind(*fifo0);
      t0->addInputPort()->bind(*fifo0);
      src1->addOutputPort()->bind(*fifo1);
      t1->addInputPort()->bind(*fifo1);

      init_PN();
    };
};

int sc_main(int argc , char *argv[]) {
  tfHapi = sc_create_vcd_trace_file (argv[0]); // creates rma.vcd

  top top1("Top1");

//  sc_start(); // start simulation until stopped by  the user
  sc_start(100000, SC_NS); // start simulation for 100 ns
  sc_close_vcd_trace_file(tfHapi);
  return 0;
}
