#include <ir/ir.h>
#include <target/util.h>
#include <stdio.h>
#include <assert.h>

static int g_indent = 0;

static const char *misty_reg_names[] = {"tv", "tp", "pc"};

typedef enum {
    TV, // val
    TP, // ptr
    PC,
    LAST_MISTY_REG
} MistyReg;

static const char *
misty_name(MistyReg reg)
{
    if (reg < LAST_MISTY_REG)
        return misty_reg_names[reg];
    // Technically 7 but not sure why PC is there
    assert(reg - LAST_MISTY_REG < 6);
    return reg_names[reg - LAST_MISTY_REG];
}

static void
misty_emit(const char *inst, ...) {
    for (int i = 0; i < g_indent; i++)
      putchar(' ');

    va_list ap;
    va_start(ap, inst);
    for (const char *ip = inst; *ip; ip++) {
        if (*ip == '%') {
            printf("%s", misty_name(va_arg(ap, MistyReg)));
        } else if (*ip == '&') {
            printf("%d", va_arg(ap, int));
        } else {
            putchar(*ip);
        }
    }
    va_end(ap);
    putchar('\n');
}

static MistyReg
misty_conv(Reg reg)
{
    return reg + LAST_MISTY_REG;
}

static MistyReg
misty_conv_reg(Value *val)
{
    if (val->type == IMM) {
        misty_emit("set % &", TV, val->imm);
        return TV;
    } else if (val->type == REG) {
        return misty_conv(val->reg);
    } else {
        error("unknown kind");
    }
}

static MistyReg
misty_reg_ptr(MistyReg reg)
{
    assert(reg != TP);
    misty_emit("set % %", TP, reg);
    return TP;
}

static void
misty_rr_mov(MistyReg dst, MistyReg src)
{
    // We could use load here too, doesn't matter
    misty_emit("load % %", misty_reg_ptr(src), dst);
}

// Like above but in situations where we clobber
static MistyReg
misty_conv_tv_reg(Value *val)
{
    if (val->type == IMM) {
        misty_emit("set % &", TV, val->imm);
    } else if (val->type == REG) {
        misty_rr_mov(TV, misty_conv(val->reg));
    } else {
        error("unknown kind");
    }
    return TV;
}

static void
misty_emit_mov(MistyReg dst, Value *src)
{
    if (src->type == IMM) {
        misty_emit("set % &", dst, src->imm);
    } else if (src->type == REG) {
        misty_rr_mov(dst, misty_conv(src->reg));
    } else {
        error("unknown kind");
    }
}

static void
misty_emit_inst(Inst *inst)
{
    printf("\n; ");
    g_indent++;
    dump_inst_fp(inst, stdout);

    switch (inst->op) {
    case MOV:
        misty_emit_mov(misty_conv(inst->dst.reg), &inst->src);
        break;

    case ADD:
        misty_emit("add % %",
            misty_reg_ptr(misty_conv(inst->dst.reg)),
            misty_conv_reg(&inst->src));
        break;

    case SUB:
        misty_conv_tv_reg(&inst->src);
        misty_emit("neg %", misty_reg_ptr(TV));
        misty_emit("add % %", misty_reg_ptr(misty_conv(inst->dst.reg)), TV);
        break;

    case LOAD:
        misty_emit("load % %", misty_conv_reg(&inst->src), misty_conv(inst->dst.reg));
        break;

    case STORE:
        // Rotated around (src <-> dst)
        misty_emit("store % %", misty_conv_reg(&inst->src), misty_conv(inst->dst.reg));
        break;

    case PUTC:
        misty_emit("#putc %", misty_conv_reg(&inst->src));
        break;

    case GETC:
        misty_emit("#getc %", misty_conv(inst->dst.reg));
        break;

    case EXIT:
        misty_emit("halt");
        break;

    case DUMP:
        break;

    case EQ:
    case NE:
    case LT:
    case GT:
    case LE:
    case GE:
        misty_emit("; na");
        break;

    case JEQ:
    case JNE:
    case JLT:
    case JGT:
    case JLE:
    case JGE:
        misty_emit("; na");
        break;

    case JMP:
        misty_emit_mov(PC, &inst->jmp);
        break;

    default:
        error("unknown opcode");
    }

    g_indent--;
}

void
target_misty(Module* module)
{
    for (int i = 0; i < LAST_MISTY_REG; i++)
        misty_emit("#def % &", i, i);
    for (int i = 0; i < 6; i++)
        misty_emit("#def % &", misty_conv(i), misty_conv(i));
    misty_emit("");

    for (Inst *inst = module->text; inst; inst = inst->next) {
        misty_emit_inst(inst);
    }
}
