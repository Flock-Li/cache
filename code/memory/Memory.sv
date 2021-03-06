`include "mips.svh"

module memory (
    exec_mreg_memory.memory in,
    memory_wreg_writeback.memory out,
    hazard_intf.memory hazard,
    exception_intf.memory exception,
    memory_dram.memory dram,
    cp0_intf.memory cp0
);
    word_t aluoutM, writedataM, readdataM;
    exec_data_t dataE;
    mem_data_t dataM;
    m_r_t mread;
    m_w_t mwrite;
    cp0_cause_t cp0_cause;
    cp0_status_t cp0_status;
    // assign cp0_cause = (cp0.cwrite.wen && cp0.cwrite.addr == 5'd13) ? cp0.cwrite.wd : cp0.cp0_data.cause;
    // assign cp0_status = (cp0.cwrite.wen && cp0.cwrite.addr == 5'd12) ? cp0.cwrite.wd : cp0.cp0_data.status;

    always_comb begin
        cp0_cause = cp0.cp0_data.cause;
        cp0_status = cp0.cp0_data.status;
        if (cp0.cwrite.wen && cp0.cwrite.addr == 5'd13) begin
            cp0_cause.IP[7:2] = cp0.cwrite.wd[15:10];
        end
        if (cp0.cwrite.wen && cp0.cwrite.addr == 5'd12) begin
            cp0_status.IM = cp0.cwrite.wd[15:8];
            cp0_status.EXL = cp0.cwrite.wd[1];
            cp0_status.IE = cp0.cwrite.wd[0];
        end
    end
    assign aluoutM = dataE.aluout;
    // assign mwrite.en = dataE.memwrite;
    assign dram.mwrite.addr = dataE.aluout;
    decoded_op_t op;
    assign op = dataE.instr.op;
    logic exception_load, exception_save, exception_sys, exception_bp;
    logic ren, wen;
    assign exception_load = ((op == LW) && (aluoutM[1:0] != '0)) ||
                            ((op == LH || op == LHU) && (aluoutM[0] != '0));
    assign exception_save = ((op == SW) && (aluoutM[1:0] != '0)) ||
                            ((op == SH) && (aluoutM[0] != '0));
    assign exception_sys = (dataE.instr.op == SYSCALL);
    assign exception_bp = (dataE.instr.op == BREAK);
    writedata writedata(.addr(aluoutM[1:0]), .op(op), ._wd(dataE.writedata),.en(wen), .wd(writedataM));
    // readdata readdata(._rd(dram.rd), .op(op), .addr(aluoutM[1:0]), .rd(readdataM));
    assign ren = dataE.instr.ctl.memtoreg;
    assign mread.ren = ren & ~exception_load;
    assign mread.addr = aluoutM;
    assign mread.size = (op == LW) ? 2'b10 : (op == LH ? 2'b01:2'b00); 
    assign mwrite.wen = wen & ~exception_save;
    assign mwrite.addr = aluoutM;
    assign mwrite.wd = writedataM;
    assign mwrite.size = (op == SW) ? 2'b10 : (op == SH ? 2'b01:2'b00);
// typedef struct packed {
//     decoded_instr_t instr;
//     word_t rd, aluout;
//     creg_addr_t writereg;
//     word_t hi, lo;
//     word_t pcplus4;
// } mem_data_t;    
    assign dataM.instr = dataE.instr;
    assign dataM.rd = dram.rd;
    assign dataM.aluout = aluoutM;
    assign dataM.writereg = dataE.writereg;
    assign dataM.hi = dataE.hi;
    assign dataM.lo = dataE.lo;
    assign dataM.pcplus4 = dataE.pcplus4;
    // ports
    // exec_mreg_memory.memory in
    assign dataE = in.dataE;

    // memory_wreg_writeback.memory out
    assign out.dataM_new = dataM;

    // hazard_intf.memory hazard
    assign hazard.dataM = dataM;
    assign hazard.is_eret = (dataE.instr.op == ERET);
    // exception_intf.memory exception
    assign exception.exception_instr = dataE.exception_instr;
    assign exception.exception_ri =  dataE.exception_ri;
    assign exception.exception_of = dataE.exception_of;
    assign exception.exception_load = exception_load;
    assign exception.exception_save = exception_save;
    assign exception.exception_sys = exception_sys;
    assign exception.exception_bp = exception_bp;
    assign exception.exception_save = exception_save;
    assign exception.in_delay_slot = dataE.in_delay_slot;
    assign exception.pc = dataE.pcplus4 - 32'd4;
    assign exception.vaddr = (dataE.exception_instr) ? exception.pc : aluoutM;
    assign exception.interrupt_info = ({exception.ext_int, 2'b00} | cp0_cause.IP | {cp0.timer_interrupt, 7'b0}) & cp0_status.IM;
    // memory_dram.memory dram    
    assign dram.mread = mread;
    assign dram.mwrite = mwrite;
    // cp0
    assign cp0.is_eret = (dataE.instr.op == ERET);
endmodule