/*
 * Copyright (C) 2010,2012 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Varun Sethi, <varun.sethi@freescale.com>
 *
 * Description:
 * This file is derived from arch/powerpc/kvm/e500.c,
 * by Yu Liu <yu.liu@freescale.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/kvm_host.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/export.h>

#include <asm/reg.h>
#include <asm/cputable.h>
#include <asm/tlbflush.h>
#include <asm/kvm_ppc.h>
#include <asm/dbell.h>
#include <asm/cputhreads.h>

#include "booke.h"
#include "e500.h"

void kvmppc_set_pending_interrupt(struct kvm_vcpu *vcpu, enum int_class type)
{
	enum ppc_dbell dbell_type;
	unsigned long tag;

	switch (type) {
	case INT_CLASS_NONCRIT:
		dbell_type = PPC_G_DBELL;
		break;
	case INT_CLASS_CRIT:
		dbell_type = PPC_G_DBELL_CRIT;
		break;
	case INT_CLASS_MC:
		dbell_type = PPC_G_DBELL_MC;
		break;
	default:
		WARN_ONCE(1, "%s: unknown int type %d\n", __func__, type);
		return;
	}

	preempt_disable();
	tag = PPC_DBELL_LPID(vcpu->arch.lpid) | vcpu->vcpu_id;
	mb();
	ppc_msgsnd(dbell_type, 0, tag);
	preempt_enable();
}

/* gtlbe must not be mapped by more than one host tlb entry */
void kvmppc_e500_tlbil_one(struct kvmppc_vcpu_e500 *vcpu_e500,
			   struct kvm_book3e_206_tlb_entry *gtlbe)
{
	unsigned int tid, ts, ind;
	gva_t eaddr;
	u32 val;
	unsigned long flags;

	ts = get_tlb_ts(gtlbe);
	tid = get_tlb_tid(gtlbe);
	ind = get_tlb_ind(&vcpu_e500->vcpu, gtlbe);

	/* We search the host TLB to invalidate its shadow TLB entry */
	val = (tid << 16) | ts | (ind << MAS6_SIND_SHIFT);
	eaddr = get_tlb_eaddr(gtlbe);

	local_irq_save(flags);

	mtspr(SPRN_MAS6, val);
	mtspr(SPRN_MAS5, MAS5_SGS | vcpu_e500->vcpu.arch.lpid);

	asm volatile("tlbsx 0, %[eaddr]\n" : : [eaddr] "r" (eaddr));
	val = mfspr(SPRN_MAS1);
	if (val & MAS1_VALID) {
		mtspr(SPRN_MAS1, val & ~MAS1_VALID);
		asm volatile("tlbwe");
	}
	mtspr(SPRN_MAS5, 0);
	/* NOTE: tlbsx also updates mas8, so clear it for host tlbwe */
	mtspr(SPRN_MAS8, 0);
	isync();

	local_irq_restore(flags);
}

void inval_ea_on_host(struct kvm_vcpu *vcpu, gva_t ea, int pid, int sas,
		      int sind)
{
	unsigned long flags;

	local_irq_save(flags);
	mtspr(SPRN_MAS5, MAS5_SGS | vcpu->arch.lpid);
	mtspr(SPRN_MAS6, (pid << MAS6_SPID_SHIFT) |
		sas | (sind << MAS6_SIND_SHIFT));
	asm volatile("tlbilx 3, 0, %[ea]\n" : :
					[ea] "r" (ea));
	mtspr(SPRN_MAS5, 0);
	local_irq_restore(flags);
}

void kvmppc_e500_tlbil_pid(struct kvm_vcpu *vcpu, int pid)
{
	unsigned long flags;

	local_irq_save(flags);
	mtspr(SPRN_MAS5, MAS5_SGS | vcpu->arch.lpid);
	mtspr(SPRN_MAS6, pid << MAS6_SPID_SHIFT);
	asm volatile("tlbilxpid");
	mtspr(SPRN_MAS5, 0);
	local_irq_restore(flags);
}

void kvmppc_e500_tlbil_lpid(struct kvm_vcpu *vcpu)
{
	unsigned long flags;

	local_irq_save(flags);
	mtspr(SPRN_MAS5, MAS5_SGS | vcpu->arch.lpid);
	asm volatile("tlbilxlpid");
	mtspr(SPRN_MAS5, 0);
	local_irq_restore(flags);
}

void inval_tlb_on_host(struct kvm_vcpu *vcpu, int type, int pid)
{
	if (type == 0)
		kvmppc_e500_tlbil_lpid(vcpu);
	else
		kvmppc_e500_tlbil_pid(vcpu, pid);
}

void kvmppc_e500_tlbil_all(struct kvmppc_vcpu_e500 *vcpu_e500)
{
	kvmppc_e500_tlbil_lpid(&vcpu_e500->vcpu);
	kvmppc_lrat_invalidate(&vcpu_e500->vcpu);
}

void kvmppc_set_pid(struct kvm_vcpu *vcpu, u32 pid)
{
	vcpu->arch.pid = pid;
}

void kvmppc_mmu_msr_notify(struct kvm_vcpu *vcpu, u32 old_msr)
{
}

static DEFINE_PER_CPU(struct kvm_vcpu *, last_vcpu_on_cpu);

void kvmppc_core_vcpu_load(struct kvm_vcpu *vcpu, int cpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int lpid_idx = 0;

	kvmppc_booke_vcpu_load(vcpu, cpu);

	/* Get current core's thread index */
	lpid_idx = mfspr(SPRN_PIR) % threads_per_core;

	vcpu->arch.lpid = vcpu->kvm->arch.lpid[lpid_idx];
	vcpu->arch.eplc = EPC_EGS | (vcpu->arch.lpid << EPC_ELPID_SHIFT);
	vcpu->arch.epsc = vcpu->arch.eplc;

	if (vcpu->arch.oldpir != mfspr(SPRN_PIR))
		pr_debug("vcpu 0x%p loaded on PID %d, lpid %d\n",
		       vcpu, smp_processor_id(), (int)vcpu->arch.lpid);

	mtspr(SPRN_LPID, vcpu->arch.lpid);
	mtspr(SPRN_EPCR, vcpu->arch.shadow_epcr);
	mtspr(SPRN_GPIR, vcpu->vcpu_id);
	mtspr(SPRN_MSRP, vcpu->arch.shadow_msrp);
	mtspr(SPRN_EPLC, vcpu->arch.eplc);
	mtspr(SPRN_EPSC, vcpu->arch.epsc);

	mtspr(SPRN_GIVPR, vcpu->arch.ivpr);
	mtspr(SPRN_GIVOR2, vcpu->arch.ivor[BOOKE_IRQPRIO_DATA_STORAGE]);
	mtspr(SPRN_GIVOR8, vcpu->arch.ivor[BOOKE_IRQPRIO_SYSCALL]);
	mtspr(SPRN_GSPRG0, (unsigned long)vcpu->arch.shared->sprg0);
	mtspr(SPRN_GSPRG1, (unsigned long)vcpu->arch.shared->sprg1);
	mtspr(SPRN_GSPRG2, (unsigned long)vcpu->arch.shared->sprg2);
	mtspr(SPRN_GSPRG3, (unsigned long)vcpu->arch.shared->sprg3);

	mtspr(SPRN_GSRR0, vcpu->arch.shared->srr0);
	mtspr(SPRN_GSRR1, vcpu->arch.shared->srr1);

	mtspr(SPRN_GEPR, vcpu->arch.epr);
	mtspr(SPRN_GDEAR, vcpu->arch.shared->dar);
	mtspr(SPRN_GESR, vcpu->arch.shared->esr);

	if (vcpu->arch.oldpir != mfspr(SPRN_PIR) ||
	    __get_cpu_var(last_vcpu_on_cpu) != vcpu) {
		kvmppc_e500_tlbil_all(vcpu_e500);
		__get_cpu_var(last_vcpu_on_cpu) = vcpu;
	}

	kvmppc_load_guest_fp(vcpu);
	kvmppc_load_guest_altivec(vcpu);
}

void kvmppc_core_vcpu_put(struct kvm_vcpu *vcpu)
{
	vcpu->arch.eplc = mfspr(SPRN_EPLC);
	vcpu->arch.epsc = mfspr(SPRN_EPSC);

	vcpu->arch.shared->sprg0 = mfspr(SPRN_GSPRG0);
	vcpu->arch.shared->sprg1 = mfspr(SPRN_GSPRG1);
	vcpu->arch.shared->sprg2 = mfspr(SPRN_GSPRG2);
	vcpu->arch.shared->sprg3 = mfspr(SPRN_GSPRG3);

	vcpu->arch.shared->srr0 = mfspr(SPRN_GSRR0);
	vcpu->arch.shared->srr1 = mfspr(SPRN_GSRR1);

	vcpu->arch.epr = mfspr(SPRN_GEPR);
	vcpu->arch.shared->dar = mfspr(SPRN_GDEAR);
	vcpu->arch.shared->esr = mfspr(SPRN_GESR);

	vcpu->arch.oldpir = mfspr(SPRN_PIR);

	kvmppc_booke_vcpu_put(vcpu);
}

int kvmppc_core_check_processor_compat(void)
{
	int r;

	if (strcmp(cur_cpu_spec->cpu_name, "e500mc") == 0)
		r = 0;
	else if (strcmp(cur_cpu_spec->cpu_name, "e5500") == 0)
		r = 0;
	else if (strcmp(cur_cpu_spec->cpu_name, "e6500") == 0)
		r = 0;
	else
		r = -ENOTSUPP;

	return r;
}

int kvmppc_core_vcpu_setup(struct kvm_vcpu *vcpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);

	vcpu->arch.shadow_epcr = SPRN_EPCR_DSIGS | SPRN_EPCR_DGTMI | \
				 SPRN_EPCR_DUVD;
#ifdef CONFIG_64BIT
	vcpu->arch.shadow_epcr |= SPRN_EPCR_ICM;
#endif
	vcpu->arch.shadow_msrp = MSRP_UCLEP | MSRP_DEP | MSRP_PMMP;

	vcpu->arch.pvr = mfspr(SPRN_PVR);
	vcpu_e500->svr = mfspr(SPRN_SVR);

	vcpu->arch.cpu_type = KVM_CPU_E500MC;

	return 0;
}

void kvmppc_core_get_sregs(struct kvm_vcpu *vcpu, struct kvm_sregs *sregs)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);

	sregs->u.e.features |= KVM_SREGS_E_ARCH206_MMU | KVM_SREGS_E_PM |
			       KVM_SREGS_E_PC;
	sregs->u.e.impl_id = KVM_SREGS_E_IMPL_FSL;

	sregs->u.e.impl.fsl.features = 0;
	sregs->u.e.impl.fsl.svr = vcpu_e500->svr;
	sregs->u.e.impl.fsl.hid0 = vcpu_e500->hid0;
	sregs->u.e.impl.fsl.mcar = vcpu_e500->mcar;

	kvmppc_get_sregs_e500_tlb(vcpu, sregs);

	sregs->u.e.ivor_high[3] =
		vcpu->arch.ivor[BOOKE_IRQPRIO_PERFORMANCE_MONITOR];
	sregs->u.e.ivor_high[4] = vcpu->arch.ivor[BOOKE_IRQPRIO_DBELL];
	sregs->u.e.ivor_high[5] = vcpu->arch.ivor[BOOKE_IRQPRIO_DBELL_CRIT];

	kvmppc_get_sregs_ivor(vcpu, sregs);
}

int kvmppc_core_set_sregs(struct kvm_vcpu *vcpu, struct kvm_sregs *sregs)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int ret;

	if (sregs->u.e.impl_id == KVM_SREGS_E_IMPL_FSL) {
		vcpu_e500->svr = sregs->u.e.impl.fsl.svr;
		vcpu_e500->hid0 = sregs->u.e.impl.fsl.hid0;
		vcpu_e500->mcar = sregs->u.e.impl.fsl.mcar;
	}

	ret = kvmppc_set_sregs_e500_tlb(vcpu, sregs);
	if (ret < 0)
		return ret;

	if (!(sregs->u.e.features & KVM_SREGS_E_IVOR))
		return 0;

	if (sregs->u.e.features & KVM_SREGS_E_PM) {
		vcpu->arch.ivor[BOOKE_IRQPRIO_PERFORMANCE_MONITOR] =
			sregs->u.e.ivor_high[3];
	}

	if (sregs->u.e.features & KVM_SREGS_E_PC) {
		vcpu->arch.ivor[BOOKE_IRQPRIO_DBELL] =
			sregs->u.e.ivor_high[4];
		vcpu->arch.ivor[BOOKE_IRQPRIO_DBELL_CRIT] =
			sregs->u.e.ivor_high[5];
	}

	return kvmppc_set_sregs_ivor(vcpu, sregs);
}

int kvmppc_get_one_reg(struct kvm_vcpu *vcpu, u64 id,
			union kvmppc_one_reg *val)
{
	int r = kvmppc_get_one_reg_e500_tlb(vcpu, id, val);
	return r;
}

int kvmppc_set_one_reg(struct kvm_vcpu *vcpu, u64 id,
		       union kvmppc_one_reg *val)
{
	int r = kvmppc_set_one_reg_e500_tlb(vcpu, id, val);
	return r;
}

void kvmppc_prepare_for_emulation(struct kvm_vcpu *vcpu, unsigned int *exit_nr)
{
	gva_t geaddr;
	hpa_t addr;
	u64 mas7_mas3;
	hva_t eaddr;
	u32 mas1, mas3;
	struct page *page;
	unsigned int addr_space, psize_shift;
	bool pr;

	if ((*exit_nr != BOOKE_INTERRUPT_DATA_STORAGE) &&
	    (*exit_nr != BOOKE_INTERRUPT_DTLB_MISS) &&
	    (*exit_nr != BOOKE_INTERRUPT_HV_PRIV) &&
	    ((*exit_nr != BOOKE_INTERRUPT_LRAT_ERROR) ||
	     (!(vcpu->arch.fault_esr & ESR_DATA))))
		return;

	/* Search guest translation to find the real addressss */
	geaddr = vcpu->arch.pc;
	addr_space = (vcpu->arch.shared->msr & MSR_IS) >> MSR_IR_LG;
	mtspr(SPRN_MAS6, (vcpu->arch.pid << MAS6_SPID_SHIFT) | addr_space);
	mtspr(SPRN_MAS5, MAS5_SGS | vcpu->arch.lpid);
	isync();
	asm volatile("tlbsx 0, %[geaddr]\n" : : [geaddr] "r" (geaddr));
	mtspr(SPRN_MAS5, 0);
	mtspr(SPRN_MAS8, 0);	

	mas1 = mfspr(SPRN_MAS1);
	if (!(mas1 & MAS1_VALID)) {
		/*
	 	 * There is no translation for the emulated instruction.
		 * Simulate an instruction TLB miss. This should force the host
		 * or ultimately the guest to add the translation and then
		 * reexecute the instruction.
		 */
		*exit_nr = BOOKE_INTERRUPT_ITLB_MISS;
		return;
	}

	/*
	 * TODO: check permissions and return a DSI if execute permission
	 * is missing
	 */
	mas3 = mfspr(SPRN_MAS3);
	pr = vcpu->arch.shared->msr & MSR_PR;
	if ((pr && (!(mas3 & MAS3_UX))) || ((!pr) && (!(mas3 & MAS3_SX))))
		WARN_ON_ONCE(1);

	/* Get page size */
	if (MAS0_GET_TLBSEL(mfspr(SPRN_MAS0)) == 0)
		psize_shift = PAGE_SHIFT;
	else
		psize_shift = MAS1_GET_TSIZE(mas1) + 10;

	mas7_mas3 = (((u64) mfspr(SPRN_MAS7)) << 32) |
		    mfspr(SPRN_MAS3);
	addr = (mas7_mas3 & (~0ULL << psize_shift)) |
	       (geaddr & ((1ULL << psize_shift) - 1ULL));

	/* Map a page and get guest's instruction */
	page = pfn_to_page(addr >> PAGE_SHIFT);
	eaddr = (unsigned long)kmap_atomic(page);
	eaddr |= addr & ~PAGE_MASK;
	vcpu->arch.last_inst = *(u32 *)eaddr;
	kunmap_atomic((u32 *)eaddr);
}

struct kvm_vcpu *kvmppc_core_vcpu_create(struct kvm *kvm,
					 unsigned int id)
{
	struct kvmppc_vcpu_e500 *vcpu_e500;
	struct kvm_vcpu *vcpu;
	int err;

	vcpu_e500 = kmem_cache_zalloc(kvm_vcpu_cache, GFP_KERNEL);
	if (!vcpu_e500) {
		err = -ENOMEM;
		goto out;
	}
	vcpu = &vcpu_e500->vcpu;

	/* Invalid PIR value -- this LPID dosn't have valid state on any cpu */
	vcpu->arch.oldpir = 0xffffffff;

	err = kvm_vcpu_init(vcpu, kvm, id);
	if (err)
		goto free_vcpu;

	err = kvmppc_e500_tlb_init(vcpu_e500);
	if (err)
		goto uninit_vcpu;

	vcpu->arch.shared = (void *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
	if (!vcpu->arch.shared)
		goto uninit_tlb;

	return vcpu;

uninit_tlb:
	kvmppc_e500_tlb_uninit(vcpu_e500);
uninit_vcpu:
	kvm_vcpu_uninit(vcpu);

free_vcpu:
	kmem_cache_free(kvm_vcpu_cache, vcpu_e500);
out:
	return ERR_PTR(err);
}

void kvmppc_core_vcpu_free(struct kvm_vcpu *vcpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);

	free_page((unsigned long)vcpu->arch.shared);
	kvmppc_e500_tlb_uninit(vcpu_e500);
	kvm_vcpu_uninit(vcpu);
	kmem_cache_free(kvm_vcpu_cache, vcpu_e500);
}

int kvmppc_core_init_vm(struct kvm *kvm)
{
	int i, lpid;

	if (threads_per_core > 2)
		return -ENOMEM;

	/* Each VM allocates one LPID per HW thread index */
	for(i = 0; i < threads_per_core; i++) {
		lpid = kvmppc_alloc_lpid();
		if (lpid < 0)
			return lpid;

		kvm->arch.lpid[i] = lpid;
	}

	return 0;
}

void kvmppc_core_destroy_vm(struct kvm *kvm)
{
	int i;

	for(i = 0; i < threads_per_core; i++) {
		kvmppc_free_lpid(kvm->arch.lpid[i]);
	}
}

static int __init kvmppc_e500mc_init(void)
{
	int r;

	r = kvmppc_booke_init();
	if (r)
		return r;

	kvmppc_init_lpid(64);
	kvmppc_claim_lpid(0); /* host */

	return kvm_init(NULL, sizeof(struct kvmppc_vcpu_e500), 0, THIS_MODULE);
}

static void __exit kvmppc_e500mc_exit(void)
{
	kvmppc_booke_exit();
}

module_init(kvmppc_e500mc_init);
module_exit(kvmppc_e500mc_exit);
