#include "util.h"
#include "VTop.h"
#include "TopBase.h"
#include "CacheBusMemory.h"
#include "verilated_fst_c.h"

#include <cassert>

constexpr int MEMORY_DEPTH = 256;

class CacheBusSlave : public ICacheBusSlave {
public:
    CacheBusSlave(VTop *inst) : _inst(inst) {}

    virtual auto valid() -> u8 {
        return _inst->cbus_req_x_valid;
    }
    virtual auto is_write() -> u8 {
        return _inst->cbus_req_x_is_write;
    }
    virtual auto addr() -> u32 {
        return _inst->cbus_req_x_addr;
    }
    virtual auto order() -> u32 {
        return _inst->cbus_req_x_order;
    }
    virtual auto wdata() -> u32 {
        return _inst->cbus_req_x_wdata;
    }

    virtual auto okay() -> u8& {
        return _inst->cbus_resp_x_okay;
    }
    virtual auto last() -> u8& {
        return _inst->cbus_resp_x_last;
    }
    virtual auto rdata() -> u32& {
        return _inst->cbus_resp_x_rdata;
    }
private:
    VTop *_inst;
};

class Top : public TopBase {
public:
    Top() {
        _bus = new CacheBusSlave(inst);
        _mem = new CacheBusMemory(MEMORY_DEPTH, _bus);
    }
    ~Top() {
        delete _bus;
        delete _mem;
    }

    void reset() {
        _tickcount = 0;
        _mem->reset();

        inst->clk = 0;
        inst->reset = 0;
        inst->sramx_req_x_addr = 0;
        inst->sramx_req_x_req = 0;
        inst->sramx_req_x_size = 0;
        inst->sramx_req_x_wdata = 0;
        inst->sramx_req_x_wr = 0;
        inst->cbus_resp_x_last = 0;
        inst->cbus_resp_x_okay = 0;
        inst->cbus_resp_x_rdata = 0;

        inst->reset = 1;
        tick();
        inst->reset = 0;
        tick();
    }

    void clock_trigger() {
        _mem->trigger();
    }

    void post_clock_hook() {
        _mem->eval();
    }

    void issue_read(int order, int addr) {
        inst->sramx_req_x_req = 1;
        inst->sramx_req_x_wr = 0;
        inst->sramx_req_x_size = order;
        inst->sramx_req_x_addr = addr;
        inst->sramx_req_x_wdata = 0xcccccccc;
    }

    void print_cache() {
        for (int i = 0; i < 16; i++) {
            printf("%x ", inst->mem[i]);
        }
        puts("");
    }

private:
    CacheBusSlave *_bus;
    CacheBusMemory *_mem;
};

static auto top = new Top;

WITH {
    top->reset();
    top->issue_read(2, 4 * 7);

    top->start_trace("trace/single-read.fst");
    top->print_cache();
    for (int i = 0; i < 18; i++) {
        top->tick();
        top->print_cache();
    }

    top->stop_trace();
    assert(top->inst->sramx_resp_x_rdata == 7);
} AS("single read");