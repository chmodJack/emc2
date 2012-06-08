//*********************************************************************
// Copyright 2010, Institute of Computer and Network Engineering,
//                 TU-Braunschweig
// All rights reserved
// Any reproduction, use, distribution or disclosure of this program,
// without the express, prior written consent of the authors is 
// strictly prohibited.
//
// University of Technology Braunschweig
// Institute of Computer and Network Engineering
// Hans-Sommer-Str. 66
// 38118 Braunschweig, Germany
//
// ESA SPECIAL LICENSE
//
// This program may be freely used, copied, modified, and redistributed
// by the European Space Agency for the Agency's own requirements.
//
// The program is provided "as is", there is no warranty that
// the program is correct or suitable for any purpose,
// neither implicit nor explicit. The program and the information in it
// contained do not necessarily reflect the policy of the 
// European Space Agency or of TU-Braunschweig.
//*********************************************************************
// Title:      irqmp.cpp
//
// ScssId:
//
// Origin:     HW-SW SystemC Co-Simulation SoC Validation Platform
//
// Purpose:    implementation of irqmp module
//             is included by irqmp.h template header file
//
// Method:
//
// Modified on $Date$
//          at $Revision$
//          by $Author$
//
// Principal:  European Space Agency
// Author:     VLSI working group @ IDA @ TUBS
// Maintainer: Rolf Meyer
// Reviewed:
//*********************************************************************

/// @addtogroup irqmp
/// @{

#include "irqmp.h"
#include "verbose.h"
#include <string>

static char *powerevents[] = {
  "irq0", "irq1", "irq2", "irq3", "irq4", "irq5", "irq6", "irq7", "irq8", "irq9", "irq10", "irq11", "irq12", "irq13", "irq14", "irq15"
};

/// Constructor
Irqmp::Irqmp(sc_core::sc_module_name name, 
	     int paddr, 
	     int pmask, 
	     int ncpu, 
	     int eirq, 
	     unsigned int pindex,
	     bool powmon) :
            gr_device(name, //sc_module name
                    gs::reg::ALIGNED_ADDRESS, //address mode (options: aligned / indexed)
                    0xFF, //dword size (of register file)
                    NULL //parent module
            ),
            APBDevice(pindex, 0x01, 0x00D, 3, 0, APBDevice::APBIO, pmask, false, false, paddr),
            apb_slv(
                    "APB_SLAVE", //name
                    r, //register container
                    (paddr & pmask) << 8, // start address
                    (((~pmask & 0xfff) + 1) << 8), // register space length
                    ::amba::amba_APB, // bus type
                    ::amba::amba_LT, // communication type / abstraction level
                    false // not used
            ), 
            cpu_rst("CPU_RESET"), cpu_stat("CPU_STAT"), irq_req("CPU_REQUEST"), 
            irq_ack(&Irqmp::acknowledged_irq,"IRQ_ACKNOWLEDGE"), 
            irq_in(&Irqmp::incomming_irq, "IRQ_INPUT"), 
            g_ncpu(ncpu), g_eirq(eirq),
            m_performance_counters("performance_counters"), 
            m_irq_counter("irq_line_activity", 32, m_performance_counters),
            m_cpu_counter("cpu_line_activity", ncpu, m_performance_counters),
	          powermon(powmon) {

    m_api = gs::cnf::GCnf_Api::getApiInstance(this);
    forcereg = new uint32_t[g_ncpu];

    // Display APB slave information
    v::info << name << "APB slave @0x" << hex << v::setw(8)
            << v::setfill('0') << apb_slv.get_base_addr() << " size: 0x" << hex
            << v::setw(8) << v::setfill('0') << apb_slv.get_size() << " byte"
            << endl;

    assert(ncpu < 17 && "the IRQMP can only handle up to 16 CPUs");

    // create register | name + description        
    r.create_register("level", "Interrupt Level Register",
            // offset
            0x00,
            
            // config
            gs::reg::STANDARD_REG | gs::reg::SINGLE_IO | gs::reg::SINGLE_BUFFER
                    | gs::reg::FULL_WIDTH,
            
            // init value
            0x00000000,
            
            // write mask
            Irqmp::IR_LEVEL_IL,
            
            // reg width (maximum 32 bit)
            32,
            
            // lock mask: Not implementet, has to be zero.
            0x00);
    
    r.create_register("pending", "Interrupt Pending Register", 0x04,
            gs::reg::STANDARD_REG | gs::reg::SINGLE_IO | 
            gs::reg::SINGLE_BUFFER | gs::reg::FULL_WIDTH, 0x00000000, 
            IR_PENDING_EIP | IR_PENDING_IP, 32, 0x00);
    //Following register is part of the manual, but will never be used.
    // 1) A system with 0 cpus will never be implemented
    // 2) If there were 0 cpus, no cpu would need an IR force register
    // 3) The IR force register for the first CPU ('CPU 0') will always be located at address 0x80
    //if (g_ncpu == 0) {
    r.create_register("force", "Interrupt Force Register", 0x08,
            gs::reg::STANDARD_REG | gs::reg::SINGLE_IO
                    | gs::reg::SINGLE_BUFFER | gs::reg::FULL_WIDTH,
            0x00000000, IR_FORCE_IF, 32, 0x00);
    //}
    r.create_register("clear", "Interrupt Clear Register", 0x0C,
            gs::reg::STANDARD_REG | gs::reg::SINGLE_IO | gs::reg::SINGLE_BUFFER
                    | gs::reg::FULL_WIDTH, 0x00000000, IR_CLEAR_IC, 32,
            0x00);
    r.create_register("mpstat", "Multiprocessor Status Register", 0x10,
            gs::reg::STANDARD_REG | gs::reg::SINGLE_IO | gs::reg::SINGLE_BUFFER
                    | gs::reg::FULL_WIDTH, 0x00000000, MP_STAT_WMASK, 32, 0x00);
    r.create_register("broadcast", "Interrupt broadcast Register", 0x14,
            gs::reg::STANDARD_REG | gs::reg::SINGLE_IO | gs::reg::SINGLE_BUFFER
                    | gs::reg::FULL_WIDTH, 0x00000000, BROADCAST_BM, 32,
            0x00);

    for (int i = 0; i < g_ncpu; ++i) {
        r.create_register(gen_unique_name("mask", false),
                "Interrupt Mask Register", 0x40 + 0x4 * i,
                gs::reg::STANDARD_REG | gs::reg::SINGLE_IO
                        | gs::reg::SINGLE_BUFFER | gs::reg::FULL_WIDTH,
                0xFFFFFFFE, PROC_MASK_EIM | Irqmp::PROC_MASK_IM, 32, 0x00);
        r.create_register(gen_unique_name("force", false),
                "Interrupt Force Register", 0x80 + 0x4 * i,
                gs::reg::STANDARD_REG | gs::reg::SINGLE_IO
                        | gs::reg::SINGLE_BUFFER | gs::reg::FULL_WIDTH,
                0x00000000, PROC_IR_FORCE_IFC | PROC_IR_FORCE_IF, 32,
                0x00);
        r.create_register(gen_unique_name("eir_id", false),
                "Extended Interrupt Identification Register", 0xC0 + 0x4 * i,
                gs::reg::STANDARD_REG | gs::reg::SINGLE_IO
                        | gs::reg::SINGLE_BUFFER | gs::reg::FULL_WIDTH,
                0x00000000, PROC_EXTIR_ID_EID, 32, 0x00);
        // Reset corresponding performance counter
        m_cpu_counter[i] = 0;
        forcereg[i] = 0;
    }

    SC_THREAD(launch_irq);

    PM::registerIP(this, "irqmp", powermon);
    PM::send_idle(this, "idle", sc_time_stamp(), true);
    
    // Initialize performance counter by zerooing
    // Keep in mind counter will not be reseted in reset 
    for(int i = 0; i < 32; i++) {
        m_irq_counter[i] = 0;
    }

    // Configuration report
    v::info << this->name() << " ******************************************************************************* " << v::endl;
    v::info << this->name() << " * Created configuration report with following parameters: " << v::endl;
    v::info << this->name() << " * ------------------------------------------------------- " << v::endl;
    v::info << this->name() << " * paddr/pmask: " << hex << paddr << "/" << pmask << v::endl;
    v::info << this->name() << " * ncpu: " << ncpu << v::endl;
    v::info << this->name() << " * eirq: " << eirq << v::endl;
    v::info << this->name() << " * pindex: " << pindex << v::endl;
    v::info << this->name() << " ******************************************************************************* " << v::endl;
    
}

Irqmp::~Irqmp() {
    GC_UNREGISTER_CALLBACKS();
    delete[] forcereg;
}

void Irqmp::end_of_elaboration() {
    // send interrupts to processors after write to pending / force regs
    GR_FUNCTION(Irqmp, pending_write); // args: module name, callback function name
    GR_SENSITIVE(r[IR_PENDING].add_rule(gs::reg::POST_WRITE,"pending_write", gs::reg::NOTIFY));
    GR_SENSITIVE(r[IR_FORCE].add_rule(gs::reg::POST_WRITE,"force_write", gs::reg::NOTIFY));
    for(int i_cpu = 0; i_cpu < g_ncpu; i_cpu++) {
        GR_SENSITIVE(r[PROC_IR_MASK(i_cpu)].add_rule(gs::reg::POST_WRITE,"proc_ir_mask_write", gs::reg::NOTIFY));
    }

    // unset pending bits of cleared interrupts
    GR_FUNCTION(Irqmp, clear_write);
    GR_SENSITIVE(r[IR_CLEAR].add_rule(gs::reg::POST_WRITE, "clear_write", gs::reg::NOTIFY));

    // unset force bits of cleared interrupts
    for(int i_cpu = 0; i_cpu < g_ncpu; i_cpu++) {
        GR_FUNCTION(Irqmp, force_write);
        GR_SENSITIVE(r[PROC_IR_FORCE(i_cpu)].add_rule(
                gs::reg::POST_WRITE, gen_unique_name("force_write", false), gs::reg::NOTIFY));
    }

    // manage cpu run / reset signals after write into MP status reg
    GR_FUNCTION(Irqmp, mpstat_write);
    GR_SENSITIVE(r[MP_STAT].add_rule(gs::reg::POST_WRITE, "mpstat_write", gs::reg::NOTIFY));

    GR_FUNCTION(Irqmp, mpstat_read);
    GR_SENSITIVE(r[MP_STAT].add_rule(gs::reg::PRE_READ, "mpstat_read", gs::reg::NOTIFY));
}

// Print execution statistic at end of simulation
void Irqmp::end_of_simulation() {

  v::report << name() << " ********************************************" << v::endl;
  v::report << name() << " * IRQMP statistic:" << v::endl;
  v::report << name() << " * ================" << v::endl;
  uint64_t sum = 0;
  for(int i = 1; i < 32; i++) {
    v::report << name() << " * + IRQ Line " << std::dec << i << ":    " << m_irq_counter[i] << v::endl;
    sum += m_irq_counter[i];
  }
  v::report << name() << " * -------------------------------------- " << v::endl;
  v::report << name() << " * = Sum      :    " << sum << v::endl;
  v::report << name() << " * " << v::endl;
  sum = 0;
  for(int i = 0; i < g_ncpu; i++) {
    v::report << name() << " * + CPU Line " << std::dec << i << ":    " << m_cpu_counter[i] << v::endl;
    sum += m_cpu_counter[i];
  }
  v::report << name() << " * -------------------------------------- " << v::endl;
  v::report << name() << " * = Sum      :    " << sum << v::endl;
  v::report << name() << " ******************************************** " << v::endl;

}

// Reset registers to default values
// Process sensitive to reset signal
void Irqmp::dorst() {
    //initialize registers with values defined above
    r[IR_LEVEL]   = static_cast<uint32_t>(LEVEL_DEFAULT);
    r[IR_PENDING] = static_cast<uint32_t>(PENDING_DEFAULT);
    if(g_ncpu == 0) {
        r[IR_FORCE] = static_cast<uint32_t>(FORCE_DEFAULT);
    }
    r[IR_CLEAR] = static_cast<uint32_t>(CLEAR_DEFAULT);
    //mp status register contains g_ncpu and g_eirq at bits 31..28 and 19..16 respectively
    r[MP_STAT] = 0xFFFE | (g_ncpu << 28) | (g_eirq << 16);
    r[BROADCAST] = static_cast<uint32_t>(BROADCAST_DEFAULT);
    for(int cpu = 0; cpu < g_ncpu; cpu++) {
        r[PROC_IR_MASK(cpu)]  = static_cast<uint32_t>(MASK_DEFAULT);
        r[PROC_IR_FORCE(cpu)] = static_cast<uint32_t>(PROC_FORCE_DEFAULT);
        r[PROC_EXTIR_ID(cpu)] = static_cast<uint32_t>(EXTIR_ID_DEFAULT);
        forcereg[cpu] = 0;
    }
    cpu_rst.write(1, true);
}

//  - watch interrupt bus signals (apbi.pirq)
//  - write incoming interrupts into pending or force registers
//
// process sensitive to apbi.pirq
void Irqmp::incomming_irq(const bool &value, const uint32_t &irq, const sc_time &time) {
    // A variable with true as workaround for greenreg.
    bool t = true;
    if(!value) {
        // Return if the value turned to false.
        // Interrupts will not be unset this way.
        // So we cann simply ignore a false value.
        return;
    }
    // Performance counter increase
    m_irq_counter[irq] = m_irq_counter[irq] + 1;
    
    // If the incomming interrupt is not listed in the broadcast register 
    // it goes in the pending register
    if(!r[BROADCAST].bit_get(irq)) {
        r[IR_PENDING].bit_set(irq, t);
    }
    
    // If it is not listed n the broadcast register and not an extended interrupt it goes into the force registers.
    // EIRs cannot be forced
    if(r[BROADCAST].bit_get(irq) && (irq < 16)) {
        //set force registers for broadcasted interrupts
        for(int32_t cpu = 0; cpu < g_ncpu; cpu++) { 
            r[PROC_IR_FORCE(cpu)].bit_set(irq, t);
            forcereg[cpu] |= (t << irq);
        }
    }
    // Pending and force regs are set now. 
    // To call an explicit launch_irq signal is set here
    e_signal.notify(2 * clock_cycle);
}

// launch irq:
//  - combine pending, force, and mask register
//  - prioritize pending interrupts
//  - send highest priority IR to processor via irqo port
//
// callback registered on IR pending register,
//                        IR force registers
void Irqmp::launch_irq() {
    int16_t high;
    uint32_t masked, pending, all;
    bool eirq_en;
    int cpu = 0;
    while(1) {
        wait(e_signal);
        for(cpu = g_ncpu-1; cpu > -1; cpu--) {
            // Pending register for this CPU line.
            pending = (r[IR_PENDING] | r[IR_FORCE]) & r[PROC_IR_MASK(cpu)];
            v::debug << name() << "For CPU " << cpu << " pending: " << v::uint32 << r[IR_PENDING].get() << ", force: " << v::uint32 << r[IR_FORCE].get() << ", proc_ir_mask: " << r[PROC_IR_MASK(cpu)].get() << v::endl;

            // All relevant interrupts for this CPU line to determ pending extended interrupts
            masked  = pending | (r[PROC_IR_FORCE(cpu)] & IR_FORCE_IF);
            // if any pending extended interrupts
            if(g_eirq != 0) {
                // Set the pending pit in the pending register.
                eirq_en = masked & IR_PENDING_EIP;
                r[IR_PENDING].bit_set(g_eirq, eirq_en);
            } else {
                eirq_en = 0;
            }
            // Recalculate relevant interrupts
            all = pending | (eirq_en << g_eirq) | (r[PROC_IR_FORCE(cpu)] & IR_FORCE_IF);
            v::debug << name() << "For CPU " << cpu << " pending: " << v::uint32 << pending << ", all " << v::uint32 << all << v::endl;
            
            // Find the highes not extended interrupt on level 1 
            masked = (all & r[IR_LEVEL]) & IR_PENDING_IP;
            for(high = 15; high > 0; high--) {
                if(masked & (1 << high)) {
                    break;
                }
            }

            // If no IR on level 1 found check level 0.
            if(high == 0) {
                // Find the highes not extended interrupt on level 0 
                masked = (all & ~r[IR_LEVEL]) & IR_PENDING_IP;
                for(high = 15; high > 0; high--) {
                    if(masked & (1 << high)) {
                        break;
                    }
                }
            }
            // If an interrupt is selected send it out to the CPU.
            if(high!=0) {
                v::debug << name() << "For CPU " << cpu << " send IRQ: " << high << v::endl;
                std::pair<uint32_t, bool> value(0xF & high, true);
                if(value != irq_req.read(cpu)) {
                    v::debug << name() << "For CPU " << cpu << " really sent IRQ: " << high << v::endl;
                    irq_req.write(1 << cpu, value);

                    // Depending on CPU ID emit power event
                    PM::send(this, powerevents[high], 1, sc_time_stamp(), 0, powermon);
                    PM::send(this, powerevents[high], 1, sc_time_stamp()+clock_cycle, 0, powermon);

                    m_cpu_counter[cpu]++;
                }
            } else if(irq_req.read(cpu).first!=0) {
                irq_req.write(1 << cpu, std::pair<uint32_t, bool>(0, false));
            }
        }
    }
}

//
// clear acknowledged IRQs                                           (three processes)
//  - interrupts can be cleared
//    - by software writing to the IR_Clear register                 (process 1: clear_write)
//    - by software writing to the upper half of IR_Force registers  (process 2: Clear_forced_ir)
//    - by processor sending irqi.intack signal and irqi.irl data    (process 3: clear_acknowledged_irq)
//  - remove IRQ from pending / force registers
//  - in case of eirq, release eirq ID in eirq ID register
//
// callback registered on interrupt clear register
void Irqmp::clear_write() {
    // pending reg only: forced IRs are cleared in the next function
    bool extirq = false;
    for(int cpu = 0; cpu < g_ncpu; cpu++) {
        if(g_eirq != 0 && r[PROC_EXTIR_ID(cpu)]!=0) {
            r[IR_PENDING] = r[IR_PENDING] & ~(1 << r[PROC_EXTIR_ID(cpu)]);
            r[PROC_EXTIR_ID(cpu)] = 0;
            extirq = true;
        }
    }
    for(int i = 31; i > 15; --i) {
        if((1<<i)&r[IR_CLEAR]) {
            extirq = true;
        }
    }
    if(extirq) {
        irq_req.write(~0, std::pair<uint32_t, bool>(g_eirq, false));
    }
    for(int i = 15; i > 0; --i) {
        if((1<<i)&r[IR_CLEAR]) {
            irq_req.write(~0, std::pair<uint32_t, bool>(i, false));
        }
    }
    
    uint32_t cleared_vector = r[IR_PENDING] & ~r[IR_CLEAR];
    r[IR_PENDING] = cleared_vector;
    cleared_vector = r[IR_FORCE] & ~r[IR_CLEAR];
    r[IR_FORCE] = cleared_vector;
    r[IR_CLEAR]   = 0;
    e_signal.notify(2 * clock_cycle);
}

// callback registered on interrupt force registers
void Irqmp::force_write() {
    for(int cpu = 0; cpu < g_ncpu; cpu++) {
        uint32_t reg = r[PROC_IR_FORCE(cpu)];
        for(int i = 15; i > 0; --i) {
            if((1<<i)&(reg>>16)) {
                irq_req.write(~0, std::pair<uint32_t, bool>(i, false));
            }
        }
        forcereg[cpu] |= reg;
        
        //write mask clears IFC bits:
        //IF && !IFC && write_mask
        forcereg[cpu] &= (~(forcereg[cpu] >> 16) & PROC_IR_FORCE_IF);
        
        r[PROC_IR_FORCE(cpu)] = forcereg[cpu];
        if(g_eirq != 0 && r[PROC_EXTIR_ID(cpu)]!=0) {
            r[IR_PENDING] = r[IR_PENDING] & ~(1 << r[PROC_EXTIR_ID(cpu)]);
        }
    }
    e_signal.notify(2 * clock_cycle);
}

// process sensitive to ack_irq
void Irqmp::acknowledged_irq(const uint32_t &irq, const uint32_t &cpu, const sc_core::sc_time &time) {
    bool f = false;
    if(g_eirq != 0 && r[PROC_EXTIR_ID(cpu)]!=0) {
        r[IR_PENDING] = r[IR_PENDING] & ~(1 << r[PROC_EXTIR_ID(cpu)]);
    }
    
    //clear interrupt from pending and force register
    if(r[BROADCAST].bit_get(irq)) {
            r[PROC_IR_FORCE(cpu)].bit_set(irq, f);
            forcereg[cpu] &= ~(1 << irq) & 0xFFFE;
    }
    
    irq_req.write(~0, std::pair<uint32_t, bool>(irq, false));
    r[IR_PENDING].bit_set(irq, f);
    r[IR_FORCE].bit_set(irq, f);
    r[PROC_EXTIR_ID(cpu)] = 0;
    e_signal.notify(2 * clock_cycle);
}

// reset cpus after write to cpu status register
// callback registered on mp status register
void Irqmp::mpstat_write() {
    uint32_t stat = r[MP_STAT] & 0xFFFF;
    for(int i = 0; i < g_ncpu; i++) {
        if((stat & (1 << i)) && cpu_stat.read(i)) {
            cpu_rst.write(1 << i, true);
        }
    }
}

void Irqmp::mpstat_read() {
    uint32_t reg = (g_ncpu << 28) | (g_eirq << 16);
    for(int i = 0; i < g_ncpu; i++) {
        reg |= (cpu_stat.read(i) << i);
    }
    r[MP_STAT] = reg;
}

void Irqmp::pending_write() {
    v::debug << name() << "Pending/Force write" << v::endl;
    e_signal.notify(1 * clock_cycle);
}

/// @}
