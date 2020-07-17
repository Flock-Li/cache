#include "OneLineBuffer.h"

static auto top = new Top;

PRETEST_HOOK [] {
    top->reset();
};

WITH {
    top->issue_read(2, 4 * 31);

    int count = 0;
    while (!top->inst->sramx_resp_x_data_ok) {
        top->tick();
        top->print_cache();
        count++;
        assert(count < 256);
    }

    assert(top->inst->sramx_resp_x_rdata == 31);
} AS("single read");

WITH {
    for (int i = 0; i < 256; i++) {
        top->tick();
        top->issue_read(2, 4 * i);

        int count = 0;
        while (!top->inst->sramx_resp_x_addr_ok) {
            top->tick();
            count++;
            assert(count < 100);
        }

        if (!top->inst->sramx_resp_x_data_ok) {
            top->tick();
            top->inst->sramx_req_x_req = 0;
            top->inst->eval();
            while (!top->inst->sramx_resp_x_data_ok) {
                top->tick();
                count++;
                assert(count < 100);
            }
        }

        info("%x ?= %x\n", top->inst->sramx_resp_x_rdata, i);
        assert(top->inst->sramx_resp_x_rdata == i);
    }
} AS("sequential read");

WITH {
    top->issue_write(2, 4 * 7, 0xdeadbeef);

    int count = 0;
    while (!top->inst->sramx_resp_x_data_ok) {
        top->tick();
        top->print_cache();
        count++;
        assert(count < 256);
    }

    top->tick();  // handshake at posedge

    for (int i = 0; i < 8; i++) {
        if (i == 7)
            assert(top->inst->mem[i] == 0xdeadbeef);
        else
            assert(top->inst->mem[i] == i);
    }

    assert(top->cmem->mem[7] != 0xdeadbeef);
    assert(top->cmem->mem[7] == 7);
} AS("single write");

WITH {
    u32 data[256];

    top->tick();
    for (int i = 0; i < 256; i++) {
        data[i] = randu();

        top->issue_write(2, 4 * i, data[i]);

        int count = 0;
        while (!top->inst->sramx_resp_x_addr_ok) {
            top->tick();
            assert(++count < 256);
        }

        bool data_ok = top->inst->sramx_resp_x_data_ok;
        top->tick();
        top->inst->sramx_req_x_req = 0;
        top->inst->eval();

        if (!data_ok) {
            do {
                top->tick();
                assert(++count < 256);
            } while (!top->inst->sramx_resp_x_data_ok);
            top->tick();
        }

        top->issue_read(2, 4 * i);

        count = 0;
        while (!top->inst->sramx_resp_x_addr_ok) {
            top->tick();
            assert(++count < 256);
        }

        if (!top->inst->sramx_resp_x_data_ok) {
            top->tick();
            top->inst->sramx_req_x_req = 0;
            top->inst->eval();

            while (!top->inst->sramx_resp_x_data_ok) {
                top->tick();
                count++;
                assert(count < 100);
            }
        }

        u32 rdata = top->inst->sramx_resp_x_rdata;
        top->tick();

        info("data[%d] = %08x, rdata = %08x\n", i, data[i], rdata);
        assert(rdata == data[i]);
    }

    // force writing back
    top->issue_read(2, 0);
    top->tick(256);

    for (int i = 0; i < 256; i++) {
        info("data[%d] = %08x, mem[%d] = %08x\n", i, data[i], i, top->cmem->mem[i]);
        assert(data[i] == top->cmem->mem[i]);
    }
} AS("sequential write");

WITH {
    constexpr int OP_COUNT = 500000;

    enum OpType : int {
        READ, WRITE
    };

    u32 ref_mem[256];
    for (int i = 0; i < 256; i++) {
        ref_mem[i] = i;
    }

    top->tick(8);
    for (int _t = 0; _t < OP_COUNT; _t++) {
        int op = randu(0, 1);
        int index = randu(0, 255);
        int addr = index * 4;
        u32 data = randu();

        switch (op) {
            case READ: {
                info("test: read @0x%x[%d]\n", addr, index);
                top->issue_read(2, addr);

                int count = 0;
                while (!top->inst->sramx_resp_x_addr_ok) {
                    top->tick();
                    assert(++count < 256);
                }

                if (!top->inst->sramx_resp_x_data_ok) {
                    top->tick();
                    top->inst->sramx_req_x_req = 0;
                    top->inst->eval();

                    while (!top->inst->sramx_resp_x_data_ok) {
                        top->tick();
                        assert(++count < 256);
                    }
                }

                u32 rdata = top->inst->sramx_resp_x_rdata;
                top->tick();

                info("ref_mem[%d] = %08x, rdata = %08x\n", index, ref_mem[index], rdata);
                assert(ref_mem[index] == rdata);
            } break;

            case WRITE: {
                info("test: write @0x%x[%d]: %08x\n", addr, index, data);
                ref_mem[index] = data;
                top->issue_write(2, addr, data);

                int count = 0;
                while (!top->inst->sramx_resp_x_addr_ok) {
                    top->tick();
                    assert(++count < 256);
                }

                if (!top->inst->sramx_resp_x_data_ok) {
                    top->tick();
                    top->inst->sramx_req_x_req = 0;
                    top->inst->eval();

                    while (!top->inst->sramx_resp_x_data_ok) {
                        top->tick();
                        assert(++count < 256);
                    }
                }

                top->tick();
            } break;
        };
    }

    // force writing back
    top->issue_read(2, 0);
    top->tick(256);
    top->issue_read(2, 256);
    top->tick(256);

    for (int i = 0; i < 256; i++) {
        info("ref_mem[%d] = %08x, mem[%d] = %08x\n", i, ref_mem[i], i, top->cmem->mem[i]);
        assert(ref_mem[i] == top->cmem->mem[i]);
    }
} AS("random read/write");