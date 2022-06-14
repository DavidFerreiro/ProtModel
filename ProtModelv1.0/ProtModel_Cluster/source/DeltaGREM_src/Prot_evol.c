/* Program Prot_evol.
   Needs a target structure and a starting DNA sequence.
   Attempts DNA mutations with given mutational bias.
   alpha, Z, energy are computed for the mutated sequence.
   The mutation is accepted or not according to selection criteria.
   The base composition is evaluated at every time step.
*/

int OPT_REG=1;  // Optimize the regularization parameter REG?
int SCORE_CV=0; // Score to optimize REG: 1=CV 0=-|KL_mod-KL_reg|
int MF_COMP=1;  // Perform Mean-field computations (slow)?
float REG_FACT=0.35; // Initial value of regularization
int REMUT=1;    // Repeat the fit of mutation parameters?
// Possibly problematic choices
float PMIN=0.001; // Minimal allowed value of predicted a.a. frequencies
int PMIN_ZERO=0; // Set Pmin to zero when optimizing REG?
int LAMBDA_ANALYTIC=1; // Optimize Lambda by vanishing derivative
int NORMALIZE=1;  // Normalize the exponent of Psel?

int UPDATE_LAMBDA=1; // Update initial value of lambda for each REG
//
int PRINT_TN=1; // Print Tajima-Nei divergence?
int ALL_MUTS=0; // Compute the effect of all possible DNA mutations?

float D_REG=0.01;   // increment of REG for computing Cv (specific heat)
float Lambda_start[2]={1,0}; // Initial value of Lambda
int ini_CV=1;

float REG_COEF;
float reg_ini;
int ini_print;

#define FILE_CODE_DEF "gen_code_ATGC.in"
// #define FILE_ENE_DEF  "energy.in"
#define VBT 0 // Verbatim

#include "REM.h"
#include "coord.h"
#include "gen_code.h"
#include "allocate.h"
#include "protein3.h"
#include "read_pdb.h"
#include "random3.h"           /* Generating random numbers */
#include "mutation.h"
#include "codes.h"
#include "input.h"
#include "get_para_pop_dyn.h"
#include "subst_exhaustive.h"
#include "read_str_mut.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include "Print_meanfield.h"
#include "fits.h"
#include "meanfield.h"
#include "read_ali.h"

/* New version 27/07/2021:
   Now energy parameters and genetic codes are given in header files
   energy_BKV.h gen_code.h
*/

//#define FILE_IN "Prot_evol.in"
#define N_CHAR 300          // Max. length of file names
#define EXT_MSA "_MSA.fasta" // Extension for MSA
#define EXT_SEL "_sel.dat"  // Extension for selection statistics
#define EXT_AVE "_ave.dat"  // Extension for output file with averages
#define EXT_DNA "_dna.dat"  // Extension for output file with dna statistics
#define EXT_OUT "_stab.dat" // Extension for output file with folding
                            // thermodynamics and fitness
#define EXT_FIN "_final.dat" // Extension for final results of simulation
#define EXT_SUM "_summary.dat"

int PRINT_MSA=1;
int MAX_MSA=1000;
int nseq_msa=0;

// Global variables in externals.h
// Mutation parameters
/*char coded_aa[64]="FFLLLLLLIIIMVVVVSSSSPPPPTTTTAAAAYYHHQQNNKKDDEECCWRRRRSSRRGGGG***";
  char *codon[64]={"TTT","TTC","TTA","TTG","CTT","CTC","CTA","CTG","ATT","ATC","ATA","ATG","GTT","GTC","GTA","GTG","TCT","TCC","TCA","TCG","CCT","CCC","CCA","CCG","ACT","ACC","ACA","ACG","GCT","GCC","GCA","GCG","TAT","TAC","CAT","CAC","CAA","CAG","AAT","AAC","AAA","AAG","GAT","GAC","GAA","GAG","TGT","TGC","TGG","CGT","CGC","CGA","CGG","AGT","AGC","AGA","AGG","GGT","GGC","GGA","GGG","TAA","TAG","TGA"};*/ 
#define MUTPAR 10
float mut_par[MUTPAR];
int NPAR=6;
// contact definition
float cont_thr;
// contact statistics
int DTAIL_MAX=20;
int LEN=0;
/*float *Cont_L=NULL;
float *CNc1_L=NULL;
float *CNc2_L=NULL;
float *Cnc1_L=NULL;
float **nc_nc_L=NULL;*/
char FILE_STR[200];
// Contact energy function
char AA_code[21];
float Econt[21][21];
float **Econt_T=NULL, T_ratio=1;
//float **E_loc_over_T=NULL;
float hydro[21];
float SEC_STR=1; // Coefficient for local interactions
//char SEC_EL[16];
// Thermodynamic parameters
float TEMP=0.5;
float sC1=0.065, sC0=0, sU1=0.140; // Configuration entropy
float Conf_entropy, K2Thr;
// general
int Verbose=0;
// AA distr
float Entr_ave=0, Entr_reg=0;

// Input codes
void Read_ene_par(char *, float **interactions);
int Read_ene_new(char *, float **interactions);
char *Read_sequence(int *len_dna, int *nseq, int **ini_seq, int **len_seq,
		    char *inputseq);
unsigned long randomgenerator(void);
float *Get_counts(short *seq, int L, int NAA);
int Match_dna(char *dna_seq, int nseq, int *ini_seq, int *len_seq,
	      struct protein pdb, char **codon, char *coded_aa);

// Output
void Output_name(char *file_name, char *dir_out, char *prot_name,
		 float TEMP, float sU1, float sC1,  int MEANFIELD,
		 char *MODEL, float LAMBDA, int OPT_LAMBDA, //NEW
		 int NEUTRAL, int N_pop, float *mut_par);
FILE *open_file(char *, char *, short *, int, char *fit_def);
int Print_dna(char *, FILE *, int);
void Print_ave(FILE *file_ave,
	       long it_sum, long t_indip,
	       int N_pop,
	       double f_sum, double f_dev,
	       double E_sum, double E_dev,
	       double DG_sum, double DG_dev,
	       long num_syn_subst, long num_aa_subst,
	       long num_syn_mut, long num_aa_mut,
	       float seq_entr,
	       struct load mut_load,
	       struct load trans_load);
void Print_TN_div(short *aa_seq, short *aa_seq0, int L,
		  int num_aa_subst, FILE *file_out);
void Print_seq(FILE *file_msa, short *aa_seq, int len_amm, int *nseq_msa,
	       float DeltaG);


void Print_matrix(struct protein target);
FILE *Open_summary(char *name_file, struct REM E_wt,
		   float *mut_par, short *aa_seq, float Seq_id);
void Get_mean(double *ave, double *err, float sum, float dev,
	      long it_sum, float t_indep);
void Record_evo(float *fit_evo, double fitness,
		float *DG_evo,  double DG,
		float *Enat_evo, double E_nat, int *nsam);
void Print_final(char *name_file, long it_sum,
		 float TEMP, float sU1, float sC1, float sC0,
		 int MEANFIELD, float LAMBDA, int N_pop,
		 float *mut_par, float tt_ratio,
		 //
		 float *fit_evo, float *DG_evo, 
		 float *Enat_evo, int nsam, int step,
		 double *f_ave, double f_dev, 
		 double *E_ave, double E_dev,
		 double *DG_ave, double DG_dev,
		 float seq_entr, double seq_entr_dev,
		 struct load *mut_load, struct load *trans_load,
		 long num_syn_subst, long num_aa_subst,
		 long num_syn_mut, long num_aa_mut,
		 float *dN_dS, float *accept,
		 long **nuc_evo, int len_dna, int sample);

// Calculations
void Compute_nuc_mut(char *dna_seq, int len_dna,
		     short *aa_seq, int len_amm,
		     char **codon, char *coded_aa,
		     char *SSC_TYPE, float **exp1, float **exp2,
		     float *Lambda, FILE *file_mut);
void Regularize(float **f_reg_ia, float **f_msa_ia, float *f_aa, int len_amm,
		float w_max, float reg);
float Normalize_exponent(float **SSC_mod, int L, char *model);
void Initialize_load(struct load *load);
void Sum_loads(struct load *load, struct load *load1);

int WildType_DDG(float **DG_mut, int **C_nat, int *i_sec,
		 short *aa_seq, struct REM *E_wt);

int Optimize_distr(struct MF_results *opt_res, float **P_WT_ia,
		   float **exp1, float **exp2, float *P_mut_a,
		   float **f_reg_ia, float **f_msa_ia, float *wi,
		   struct REM *E_wt, int **C_nat, int *i_sec,
		   char *name_file, FILE *file_summ, char *label,
		   int repeat);
int Analytic_Lambda(float *Lambda, float **P_WT_ia, float **exp1, float **exp2,
		    float *P_mut_a, int L, float *wi, float **f_reg_ia);
int Maximize_Lambda(float *Lambda, float **P_WT_ia, float **exp1, float **exp2,
		    float *P_mut_a, int L, float *wi, float **f_reg_ia,
		    float **f_msa_ia, int repeat);
float KL_symm(float *P, float *Q, int n);
double Optimize_reg(struct MF_results *opt_res, float **P_opt_ia,
		    float reg_ini, float w_max, float *f_aa,
		    struct MF_results *res, float **P_ia,
		    float **exp1, float **exp2, float *P_mut_a,
		    float **f_reg_ia, float **f_msa_ia, float *wi,
		    struct REM *E_wt, int **C_nat, int *i_sec,
		    char *name_file, FILE *file_summ, char *label);
double Compute_score_reg(struct MF_results *SSC_res, float reg,
			 float w_max, float *f_aa, float **P_SSC_ia,
			 float **exp1, float **exp2, float *P_mut_a,
			 float **f_reg_ia, float **f_msa_ia, float *wi,
			 struct REM *E_wt, int **C_nat, int *i_sec,
			 char *name_file, FILE *file_summ, char *label);
double Compute_Cv(struct MF_results *SSC_res, float reg, float step,
		  float w_max, float *f_aa, float **P_SSC_ia,
		  float **exp1, float **exp2, float *P_mut_a,
		  float **f_reg_ia, float **f_msa_ia, float *wi,
		  struct REM *E_wt, int **C_nat, int *i_sec,
		  char *name_file, FILE *file_summ, char *label);
int Compute_P_WT(float **P_WT_ia, float *Lambda,
		 float **exp1, float **exp2, float *P_mut_a,
		 int L, float Pmin);
int Selection(float fitness, float fitness_old, int N_pop);
int Detailed_balance(float *p, int xend, int xini);
float Sequence_entropy(double **aa_distr, int L);
float Sequence_entropy_mut(float *mut_par, char **codon, char *coded_aa);
void Compute_freq_codons(float *mut_par, float *freq_aa,
			 char **codon, char *coded_aa);
void Compute_load(double *Tload_sum, double *Tload_dev,
		  double *Mload_sum, double *Mload_dev, int *Nload,
		  struct REM *E_wt, int **C_nat, int *i_sec, short *aa_seq,
		  float fitness_wt, char *dna_seq, int len_dna, 
		  char **codon, char *coded_aa);
extern float Find_max_quad(float x1, float x2, float x3,
			   float y1, float y2, float y3,
			   float MIN, float MAX);
// Input parameters
// Mean-field model
// A: Input files defining the protein
static char dir_out[N_CHAR]; //file[N_CHAR], 
static char seq_name[N_CHAR];
static char *file_str_mut[2]; // File with structural mutation scores
// B: Thermodynamic parameters
int REM=2;   // Use 1st (1), 2nd (2) and 3rd (3) moment of misfolded energy 
float S_C, S_U;

int **C_nat;
int *i_sec; 
// C: Selection model
int Samples=10; // Number of simulated samples of evolutionary trajectories
static int IT_MAX=0; // Number of iterations for each trajectory
static int NEUTRAL=1; // Neutral (1) versus continous (0) fitness
float DG_THR_COEFF=0.95; // Threshold in DeltaG for neutral selection
static int N_pop=100;  // Population size for continuous selection model
// C2: Mean-field model
int MEANFIELD=1;  // Compute site-specific mean-field distributions
int OPT_LAMBDA=1; // 1=Optimize Lambda by maximum likelihood
float LAMBDA=0.9; // Lambda parameter for meanfield if OPT_LAMBDA=0
char MODEL[N_CHAR]="ALL"; // Type of mean-field model (ALL, NAT, DG)
float DG_OPT=-1;  // DG target of the optimization if MODEL="DG"
// D1: Mutation model, P_mut
int CpG=1;       // Enhance mutation rate at CpG dinucleotides
int GET_FREQ=3;  // 0= Get nucleotide frequencies from input
                 // 1= Fit nucleotide frequencies from AA sequence
                 // 2= Get P_mut[a] from fit plus AA sequence
                 // 3= Get P_mut[a] from AA sequence
float tt_ratio=4, kCpG=4; //mut_par[MUTPAR], 
int N_free_par=0;

// D1: Mutation model, exchangeability
char MATRIX[40]="WAG"; // Empirical exchangeability matrix
char EXCHANGE='F'; // exchangeability model. M=MUT F=FLUX Q=RATE E=EXCH
float TWONUC=0; // Rate of two to one nucleotide substitutions if MUT
// E: Output
int FORMAT=1;   // PAML format
int PRINT_E=0; // Print exchangeability matrix for all sites?

unsigned long iran;
// Data read from input files
static int len_amm=0, len_dna=0;
static int L_PDB=0; // length of conformation
static int L_ali=0;
static int L_noali=0;
static int count[4];
static float rate[4];
float DG_thr;

int main(int argc, char **argv){


  /***********************
          INPUT
  ************************/
  // Input files
  //char Input_dir[N_CHAR];
  char file_pdb[N_CHAR], file_seq[N_CHAR]="";
  char chain[200]="\0";
  char name_file[N_CHAR];
  char FILE_CODE[N_CHAR];
  char *file_ali=NULL;

  // Genetic code
  // char *codon[64], coded_aa[64], name_code[200];

  /***********************
         SEQUENCES
  ************************/
  char *dna_seq=NULL;
  short *dna_seq_short=NULL;

  /***********************
         Wild Type
  ************************/
  double fitness_wt;

  /***********************
          DUMMY
  ************************/
  int i, j, a;

  /******************** Input operations   ************************/
  sprintf(FILE_CODE, FILE_CODE_DEF);
  int N_str_mut=0;
  for(i=0; i<2; i++)file_str_mut[i]=malloc(N_CHAR*sizeof(char));
  Get_para(argc, argv, file_pdb, chain, file_seq, &file_ali,
	   file_str_mut, &N_str_mut,
	   &MEANFIELD, &MF_COMP, &OPT_REG, &SCORE_CV, &REG_FACT, 
	   FILE_STR, &TEMP, &sU1, &sC0, &sC1, &REM, &SEC_STR, 
	   &REMUT, &GET_FREQ, mut_par, &tt_ratio, &kCpG, &TWONUC,
	   &PRINT_E, &EXCHANGE, MATRIX, &FORMAT, &ALL_MUTS,
	   &IT_MAX, &Samples, &NEUTRAL, &N_pop,
	   &OPT_LAMBDA, &LAMBDA, &DG_OPT, MODEL,
	   dir_out);
  if(mut_par[6]){NPAR=7;}else{NPAR=6;}

  // Read contact matrix from PDB file
  struct residue *res; struct res_short *res_short; int *res_index;
  len_amm=Get_pdb(&target, &res, &res_short, file_pdb, chain, &res_index);
  // Output
  char dumm[400]; 
  char out_file[200]; sprintf(out_file, "%s_files.txt", target.name);
  FILE *out=fopen(out_file, "w");
  fprintf(out, "Program Prot_evol, author Ugo Bastolla (CSIC-UAM) ubastolla@cbm.csic.es\nIt simulates protein evolution with selection on the thermodynamic stability of the folded state and computes site-specific substitution matrices (frequencies and exchangeability) according to different structure- and stability-constrained models of protein evolution.\n");


  // Native contact matrix and wild-type sequence
  short *aa_seq=NULL;
  if(len_amm > 0){
    aa_seq=target.aa_seq;
    i_sec=target.i_sec;
    C_nat=Fill_C_nat(len_amm, target.contact);
    //Print_matrix(target);
  }else{
    fprintf(out, "ERROR, no protein found in file %s\n", file_pdb);
    exit(8);
    // or read contact matrix from precomputed matrices
    C_nat=Get_target(FILE_STR, file_pdb, &len_amm);
    if(len_amm>0){
      printf("contact matrix %s found in %s, %d residues\n",
	     file_pdb, FILE_STR, len_amm);
    }else{
      printf("contact matrix %s not found in %s\n",file_pdb, FILE_STR);
      fprintf(out,"contact matrix %s not found in %s\n",file_pdb, FILE_STR);
      exit(8);
    }
  }

  L_PDB=target.L_PDB;

  // Random numbers
  iran=randomgenerator();
  InitRandom( (RANDOMTYPE)iran);

  // Read DNA sequence
  if(file_seq[0]!='\0'){
    // Read from file
    char inputseq[N_CHAR]; strcpy(inputseq, file_seq);
    if(Check_file(inputseq)==0){
      printf("WARNING, DNA sequence file %s not found\n", file_seq);
    }else if((aa_seq)&&(len_amm>0)){
      int ns, *ini_seq, *len_seq;
      dna_seq=Read_sequence(&len_dna, &ns, &ini_seq, &len_seq, inputseq);
      if(dna_seq){
	if(Match_dna(dna_seq,ns,ini_seq,len_seq,target,codon,coded_aa)==0){
	  len_dna=0; free(dna_seq); dna_seq=NULL;
	}
      }
    }
    if(len_dna)
      fprintf(out, "DNA sequence of %d nuc. read in file %s\n",
	      len_dna, file_seq);
  }

  // If AA sequence not given, get it from DNA sequence
  if(aa_seq==NULL){
    if(len_dna){
      len_amm=len_dna/3;
      aa_seq=malloc(len_amm*sizeof(short));
      Translate_new(dna_seq, aa_seq, len_amm, codon, coded_aa);
    }else{
      printf("ERROR, no sequence given\n"); exit(8);
    }
  }

  // If DNA sequence not given, get it from AA sequence
  int idna=1;
  if(len_dna<=0){
    idna=0;
    printf("Drawing DNA sequence from AA sequence\n");
    fprintf(out,"Drawing DNA sequence from AA sequence\n");
    dna_seq=Extract_dna(&len_dna, len_amm, aa_seq, codon, coded_aa);
  }
  dna_seq_short=malloc(len_dna*sizeof(short));
  for(i=0; i<len_dna; i++)dna_seq_short[i]=Code_nuc(dna_seq[i]);
  float *num_dna=NULL;
  if(idna)num_dna=Get_counts(dna_seq_short, len_dna, 4);

  // Read multiple sequence alignment (amino acids)
  float *f_msa_ia[len_amm], *f_reg_ia[len_amm], wi[len_amm];
  for(i=0; i<len_amm; i++){
    f_msa_ia[i]=malloc(20*sizeof(float));
    f_reg_ia[i]=malloc(20*sizeof(float));
  }
  short **ali_seq=NULL; float Seq_id=0;
  int N_seq=Read_ali(f_msa_ia, &Seq_id, &ali_seq, &L_ali,
		     file_ali, aa_seq, len_amm, L_PDB);
  if(N_seq<=1){
    printf("WARNING, alignment not present\n"
	   "Obtaining amino acid frequencies from PDB sequence\n");
    for(i=0; i<len_amm; i++){
      for(a=0; a<20; a++)f_msa_ia[i][a]=0;
      if((aa_seq[i]<0)||(aa_seq[i]>=20)){
	printf("ERROR, wrong aa code %d at site %d\n", aa_seq[i], i);
	exit(8);
      }
      f_msa_ia[i][aa_seq[i]]=1;
    }
  }

  // Sites-global amino acid frequencies
  float num_aa[20], f_aa[20], sum=0;
  for(a=0; a<20; a++){
    num_aa[a]=0; for(i=0; i<len_amm; i++)num_aa[a]+=f_msa_ia[i][a];
    sum+=num_aa[a]; f_aa[a]=num_aa[a];
  }
  {float min=sum;
    for(a=0; a<20; a++)if((f_aa[a]>0)&&(f_aa[a]<min))min=f_aa[a];
    for(a=0; a<20; a++)if(f_aa[a]==0){f_aa[a]=min; sum+=min;}
  }
  for(a=0; a<20; a++)f_aa[a]/=sum;

  // Compute weight and entropy of each site from MSA (not yet regularized)
  Entr_ave=0; double norm=0, w_max=0; int NCmax=30;
  float Entr_ali[len_amm], h_ave[NCmax], num_nc[NCmax];
  for(i=0; i<NCmax; i++){h_ave[i]=0; num_nc[i]=0;}
  for(i=0; i<len_amm; i++){
    sum=0; for(a=0; a<20; a++)sum+=f_msa_ia[i][a];
    if(sum==0){
      printf("WARNING, PDB position %d has not been aligned\n", i);
      wi[i]=0; Entr_ali[i]=-1; h_ave[i]=0; L_noali++; continue;
    }
    wi[i]=sum; if(sum>w_max)w_max=sum;
    if(N_seq>=2){
      Entr_ali[i]=Entropy(f_msa_ia[i], 20);
      Entr_ave+=wi[i]*Entr_ali[i]; norm+=wi[i];
    }
    double h=0; for(a=0; a<20; a++)h+=f_msa_ia[i][a]*hydro[a];
    int nc=0; for(j=0; j<len_amm; j++)nc+=C_nat[i][j];
    if(nc>=NCmax)nc=NCmax-1;
    h_ave[nc]+=h/wi[i]; num_nc[nc]++;
  }

  // Output
  fprintf(out, "Target protein structure: %s %d residues %d structured\n"
	  "%d MSA columns, %d PDB residues have no associated column\n",
	  target.name, len_amm, L_PDB, L_ali, L_noali);
  fprintf(out, "Stability model based on contact interactions, parameters:\n");
  fprintf(out, "Temperature: %.3g\n", TEMP);
  fprintf(out, "Conformational entropy unfolded: %.3f L\n", sU1);
  fprintf(out, "Conformational entropy misfolded: %.3f L + %.3f\n", sC1, sC0);
  fprintf(out, "Misfolding model REM= %d\n", REM);
  fprintf(out, "Coefficient of secondary structure free energy: %.2g\n\n",
	  SEC_STR);

  if(N_seq>=2){
    Entr_ave/=norm;
    char name_ent[200]; sprintf(name_ent, "%s_entropy.dat", target.name);
    fprintf(out, "Average entropy of the MSA: %.2f\n", Entr_ave);
    fprintf(out, "Sequence entropy from alignment printed in %s\n", name_ent);
    FILE *file_ent=fopen(name_ent, "w");
    printf("Writing seq. entropy from alignment in %s\n", name_ent);
    for(i=0; i<len_amm; i++){
      fprintf(file_ent, "%c %.2f\n", AMIN_CODE[aa_seq[i]], Entr_ali[i]);
    }
    fclose(file_ent);
  }
  // Print hydrophobicity
  char name_h[200]; sprintf(name_h, "%s_hydrophobicity.dat", target.name);
  fprintf(out, "Mean hydroph. from alignment printed in %s\n", name_h);
  FILE *file_h=fopen(name_h, "w");
  printf("Writing mean hydro from alignment in %s\n", name_h);
  fprintf(file_h, "# ncont ave_hydro num\n");
  for(i=0; i<NCmax; i++){
    if(num_nc[i]==0)continue;
    fprintf(file_h, "%d %.3f %.0f\n", i, h_ave[i]/num_nc[i], num_nc[i]);
  }
  fclose(file_h);

  // Regularize and normalize the frequencies
  //float reg=REG_FACT/(sqrt(w_max)+REG_FACT);
  //printf("Regularization= %.2g/(sqrt(N_seq)+%.2g), N_seq=%.2g\n",
  //	 REG_FACT, REG_FACT, w_max);
  char reg_out[100];
  reg_ini=REG_FACT*(1.-Entr_ave/log(20.));
  REG_COEF=REG_FACT/reg_ini;
  sprintf(reg_out,
	  "Regularization= %.2g(1-Mean(entropy)/ln(20))=%.3g, Entr=%.3g\n",
	  REG_FACT, reg_ini, Entr_ave);
  //reg_ini=REG_FACT;
  //sprintf(reg_out, "Regularization= %.2g\n", reg_ini);
  printf("%s", reg_out); fprintf(out,"%s", reg_out);
  /*float reg_min=0.001;
    if(reg<reg_min){
    printf("WARNING, too small regularization (%.3g) using default %.3g\n",
    reg, reg_min); reg=reg_min;
    }*/
  Regularize(f_reg_ia, f_msa_ia, f_aa, len_amm, w_max, reg_ini);

  // Entropy of regularized frequencies
  Entr_reg=0; norm=0;
  for(i=0; i<len_amm; i++){
    Entr_reg+=wi[i]*Entropy(f_reg_ia[i], 20); norm+=wi[i];
  }
  Entr_reg/=norm;

  // Read structural effects of mutations
  char *STR_MUT_T[2];
  float **Str_mod[2]; j=0;
  for(i=0; i<N_str_mut; i++){
    STR_MUT_T[i]=malloc(80*sizeof(char));
    Str_mod[j]=Str_mut_matrix(STR_MUT_T[j],file_str_mut[i],target,res_index);
    if(Str_mod[j])j++;
  }
  if(j!=N_str_mut){
    printf("WARNING, %d str_mut files given as input but only %d found\n",
	   N_str_mut, j); N_str_mut=j;
  }


  /**************************  End INPUT  ****************************/

  /******************** Thermodynamics of wild type **********************/
  S_C=sC0+len_amm*sC1; S_U=len_amm*sU1;
  struct REM E_wt;

  Initialize_E_REM(&E_wt, len_amm, REM, TEMP, S_C, S_U, FILE_STR, 0);
  Test_contfreq(&E_wt, aa_seq, C_nat, i_sec, target.name);
  E_wt.DeltaG=Compute_DG_overT_contfreq(&E_wt, aa_seq, C_nat, i_sec, 1);

  char nameout[300]; sprintf(nameout, "%s_DeltaG.dat", target.name);
  double T0=Print_DG_contfreq(&E_wt, nameout);

  printf("T= %.2g sU1= %.2g sC1= %.2g sC0= %.2g L=%d\n",
	 TEMP, sU1, sC1, sC0, len_amm);
  printf("DeltaG/T= %.2f\n", E_wt.DeltaG);
  fprintf(out, "DeltaG/T= %.2f printed in file %s\n", E_wt.DeltaG, nameout);
  fprintf(out, "Temperature with minimum DG: T=%.2f\n", T0);
  sprintf(dumm, "%s_Threading.dat", target.name);
  fprintf(out, "Statistics of misfolded state printed in %s\n",dumm);
  sprintf(dumm,"DG= %.4g E_nat= %.4g G_misf= %.4g G_unf= %.4g\n",
	  E_wt.DeltaG, E_wt.E_nat, E_wt.G_misf, -E_wt.S_U);
  printf("%s",dumm); fprintf(out, "%s", dumm);

  if(E_wt.DeltaG > 0){
    sprintf(dumm,"WARNING, unstable target structure!\n");
    printf("%s",dumm); fprintf(out, "%s", dumm);
    if(NEUTRAL){
      sprintf(dumm, "This condition is illegal with NEUTRAL evolution, "
	      "please change the input file\n");
      printf("%s",dumm); fprintf(out, "%s", dumm);
      exit (8);
    }
  }

  // Stability of the multiple alignment
  if(ali_seq){
    char *ptr=file_ali, *p=ptr;
    while(*ptr!='\0'){if(*ptr=='/')p=ptr+1; ptr++;}
    char nameout[100]; sprintf(nameout, "%s.DeltaG", p);
    FILE *file_out=fopen(nameout,"w");
    printf("Writing DeltaG of multiple sequence alignment in %s\n", nameout);
    fprintf(out, "Writing DeltaG of multiple sequence alignment %s in %s\n",
	    p, nameout);
    struct REM E_mut;
    Initialize_E_REM(&E_mut, len_amm, REM, TEMP, S_C, S_U, FILE_STR, 0);
    for(i=0; i<N_seq; i++){
      E_mut.DeltaG=
	Compute_DG_overT_contfreq(&E_mut, ali_seq[i], C_nat, i_sec, 0);
      fprintf(file_out, "%.4g\n", E_mut.DeltaG);
    }
    fclose(file_out);
    printf("End DeltaG computations for multiple alignment\n");
  }

  // Allocate matrix
  float **P_MF_ia=Allocate_mat2_f(len_amm, 20, "P_MF");
  float **P_Str_ia[N_str_mut];

  if(MEANFIELD==0 && (ALL_MUTS==0 || idna==0))goto Simulate;

  /**************************************************************

              Computation of SSCPE substitution matrices

  ****************************************************************/

  /*********** Compute background amino acid frequencies P_mut *******/
  // Mutation parameters
  if(kCpG<tt_ratio)kCpG=tt_ratio;
  if(TWONUC>1){
    printf("WARNING, twonuc= %.1g not allowed\n",TWONUC); TWONUC=0;
  }
  mut_par[4]=kCpG; mut_par[5]=tt_ratio; mut_par[6]=TWONUC;
  float **Q_cod=Allocate_mat2_f(64, 64, "Codons"), P_cod[64];
  // Output: global amino acid frequencies
  float P_mut_a[20];

  /* Obtain global amino acid frequencies:
     if(GET_FREQ==0), from input mutation model
     if(GET_FREQ==1), from optimized mutation model
     if(GET_FREQ==2), combine optimized mutation model and a.a. frequencies
     if(GET_FREQ==3), from a.a. frequencies in MSA or protein sequence
     if (idna) Obtain nucleotide frequencies from input DNA sequence
     else get mutation parameters fitting amino acid frequencies
  */
  // Mutation model
  Get_mut_par(mut_par, P_mut_a, P_cod, Q_cod, GET_FREQ,
	      num_aa, len_amm, num_dna, target.name, 0);
  kCpG=mut_par[4]; tt_ratio=mut_par[5]; TWONUC=mut_par[6];
  printf("Mutation model ready\n");
  if(GET_FREQ==3){
    for(int a=0; a<20; a++)P_mut_a[a]=f_aa[a];
  }

  /************************* Output files ***************************/
  //Output_name(name_file, dir_out, target.name, TEMP, sU1, sC1, MEANFIELD,
  //	      MODEL, LAMBDA, OPT_LAMBDA, NEUTRAL, N_pop, mut_par);
  sprintf(name_file, "%s_SSCPE", target.name);
  FILE *file_summ=Open_summary(target.name, E_wt, mut_par, aa_seq, Seq_id);
  sprintf(dumm, "%s%s", target.name, EXT_SUM);
  fprintf(out, "Summary results printed in file %s\n", dumm);

  // Print model set-up
  fprintf(out,"Stability and structure constrained (SSC)");
  fprintf(out," site-specific substitution processes\n");
  char Freq_mod[400], tmp[80];
  sprintf(Freq_mod, "# Global a.a. frequencies");
  if(GET_FREQ==3){
    sprintf(tmp, " obtained from %d MSA seq. (+F)\n", N_seq);
    strcat(Freq_mod, tmp);
    N_free_par=19;
  }else{
    strcat(Freq_mod, " computed from mutation model");
    if(GET_FREQ){
      strcat(Freq_mod," optimized from the data\n");
      N_free_par=3;
    }else{
      strcat(Freq_mod," given from input\n");
    }
    strcat(Freq_mod,"# Nucleotide frequencies: ");
    for(i=0; i<4; i++){
      sprintf(tmp, " %c %.3f", Nuc_code(i), mut_par[i]);
      strcat(Freq_mod, tmp);
    }
    sprintf(tmp," trasition-transversion ratio: %.3f", tt_ratio);
    strcat(Freq_mod, tmp);
    if(GET_FREQ && tt_ratio!=1)N_free_par++;
    sprintf(tmp, " CpG ratio: %.3f", kCpG);
    strcat(Freq_mod, tmp);
    if(GET_FREQ && kCpG!=1)N_free_par++;
    sprintf(tmp, "Double nucleotide mutations: %.3f\n", TWONUC);
    strcat(Freq_mod, tmp);
    if(GET_FREQ && TWONUC)N_free_par++;
  }
  
  if(REMUT && GET_FREQ)
    strcat(Freq_mod,"# Global frequencies optimized iteratively 2 times\n");
  sprintf(tmp, "# Number of free parameters: %d+1\n", N_free_par);
  strcat(Freq_mod, tmp);
  fprintf(out, "%s", Freq_mod);
  fprintf(file_summ, "%s", Freq_mod);

  char SSC_TYPE[80], SSC_TYPE_OPT[80], STAB_TYPE[80];
  fprintf(out, "\nSelection Models:\n");
  fprintf(out, "Stability constrained: Mean-field (MF), wild-type (WT)\n");
  for(int i=0; i<N_str_mut; i++){
    if(Str_mod[i])fprintf(out, "Structure constrained: %s\n", STR_MUT_T[i]);
  }
  if(N_str_mut>1){
    fprintf(out, "Best Structure constrain + best stab. constrain\n");
  }else if(N_str_mut==1){
    fprintf(out, "Structure and stability constrained: %s+Best stab\n",
	    STR_MUT_T[0]);
  }

  if(N_seq){
    fprintf(file_summ,"# %d aligned sequences read in file %s\n",
	    N_seq, file_ali);
    fprintf(file_summ,"# Entropy per site of the alignment: %.3f\n",
	    Entr_ave);
    fprintf(file_summ,"# Regularized entropy per site: %.3f\n",
	    Entr_reg);
  }else{
    fprintf(file_summ,"# No aligned sequences were input\n");
  }
  fprintf(file_summ, "# Lambda optimized by ");
  if(LAMBDA_ANALYTIC){fprintf(file_summ, "equating derivative to zero\n");}
  else{fprintf(file_summ, "numerically minimizing KL_symm\n");}
  if(OPT_REG){
    fprintf(file_summ,"# Regularization parameter optimized by ");
    if(SCORE_CV){fprintf(file_summ,"mazimizing dlik/dREG");}
    else{fprintf(file_summ,"symmetrizing KL divergence");}
    fprintf(file_summ," initial value= %.3g\n", REG_FACT);
  }else{
    fprintf(file_summ,"# Regularization parameter = %.3g", REG_FACT);
  }
  fprintf(file_summ,"# Model Lambda0 Lambda1 lik(MSA) KL(mod,reg.obs) ");
  fprintf(file_summ,"KL(reg.obs,mod) entropy KL(mod,mut) DG Tf h\n");

  int SCO=-1; // Optimal structure constrained model
  int SSCO=-1; // Optimal stability and structure constrained model

  // Mutation model
  struct MF_results mut_res;
  mut_res.Lambda[0]=0; mut_res.Lambda[1]=0; mut_res.KL_mut=0;
  //mut_res.h=0; for(a=0; a<20; a++)mut_res.h+=P_mut_a[a]*hydro[a];
  for(i=0; i<len_amm; i++)for(a=0; a<20; a++)P_MF_ia[i][a]=P_mut_a[a];
  Test_distr(&mut_res, P_MF_ia, f_reg_ia, f_msa_ia, wi,
	     len_amm, C_nat, i_sec, E_wt);
  Print_results(mut_res, "mut", file_summ);

  // Wild-type model
  float **P_WT_ia=Allocate_mat2_f(len_amm, 20, "P_WT");
  float **WT_mod=Allocate_mat2_f(len_amm, 20, "WT_exp");
  float **Stab_mod_opt=WT_mod;
  struct MF_results WT_res, *Stab_res=&WT_res;
  WT_res.Lambda[1]=0;

  // Wild-type model
  WildType_DDG(WT_mod, C_nat, i_sec, aa_seq, &E_wt);
  printf("Optimizing Lambda for WT model\n");
  float Qmax=Normalize_exponent(WT_mod, len_amm, "WT");
  if(Qmax>0){
    Optimize_distr(&WT_res, P_WT_ia, WT_mod, NULL, P_mut_a,
		   f_reg_ia, f_msa_ia, wi, &E_wt, C_nat, i_sec,
		   name_file, file_summ, "WT ", 1);
    printf("Opt: %.3g %.3g\n", WT_res.score, WT_res.Lambda[0]);
  }

  if(MEANFIELD==0 && ALL_MUTS==0 && idna)goto Compute_all_muts;

  // Mean-field model
  struct MF_results MF_res; MF_res.Lambda[1]=0;
  float **MF_mod=Allocate_mat2_f(len_amm,20,"MF_exp");
  if(MF_COMP){
    if(OPT_LAMBDA==0){
      fprintf(out, "Mean-field model computed with fix Lambda= %.3f\n",LAMBDA);
      Fixed_Lambda(&MF_res, P_MF_ia, P_mut_a, C_nat, i_sec, f_reg_ia, f_msa_ia,
		   wi, E_wt, LAMBDA, name_file);
    }else{
      Optimize_Lambda(&MF_res, P_MF_ia, P_mut_a,
		      C_nat, i_sec, f_reg_ia, f_msa_ia, wi, aa_seq, E_wt,
		      DG_OPT, GET_FREQ, MODEL, name_file, file_summ);
      fprintf(out, "Mean-field model computed with optimized Lambda= %.3f\n",
	      MF_res.Lambda[0]);
    }
    // Mutation matrix derived from MF model MF_mod
    float l_Pmut[20]; for(a=0; a<20; a++)l_Pmut[a]=log(P_mut_a[a]);
    for(i=0; i<len_amm; i++){
      for(a=0; a<20; a++)MF_mod[i][a]=-log(P_MF_ia[i][a])+l_Pmut[a];
    }
    Qmax=Normalize_exponent(MF_mod, len_amm, "MF");
  }

  // Best stability constrained model
  float **P_Stab_ia; 
  if(MF_COMP && MF_res.score>WT_res.score && Qmax>0){
    strcpy(STAB_TYPE, "MF"); Stab_mod_opt=MF_mod;
    P_Stab_ia=P_MF_ia; Stab_res=&MF_res;
  }else{
    strcpy(STAB_TYPE, "WT"); Stab_mod_opt=WT_mod;
    P_Stab_ia=P_WT_ia; Stab_res=&WT_res;
  }

  // Structural effects model
  for(j=0; j<N_str_mut; j++)
    P_Str_ia[j]=Allocate_mat2_f(len_amm, 20,"P_Str_ia");
  float **SSC_mod=Allocate_mat2_f(len_amm, 20,"Str_exp");
  float **P_SSC_ia=Allocate_mat2_f(len_amm, 20,"P_SSC_ia");
  struct MF_results Str_res[2];
  Str_res[0].Lambda[1]=0; Str_res[1].Lambda[1]=0;

    
  SCO=-1; // Best structure constrained model
  for(j=0; j<N_str_mut; j++){
    // Structural effect model
    printf("Optimizing Lambda for %s model\n", STR_MUT_T[j]);
    Qmax=Normalize_exponent(Str_mod[j], len_amm, STR_MUT_T[j]);
    if(Qmax>0){
      Optimize_distr(&Str_res[j], P_Str_ia[j], Str_mod[j], NULL, P_mut_a,
		     f_reg_ia, f_msa_ia, wi, &E_wt, C_nat, i_sec,
		     name_file, file_summ, STR_MUT_T[j], 1);
      printf("Opt: %.3g %.3g\n", Str_res[j].score, Str_res[j].Lambda[0]);
      if(isnan(Str_res[j].score))continue;
      if((SCO<0)||(Str_res[j].score>Str_res[SCO].score))SCO=j;
    }
  }

  // Structure and stability constrained models SSC
  float **SSC_mod_opt=Allocate_mat2_f(len_amm, 20,"exp_SSC_ia");
  float **P_SSC_opt_ia=Allocate_mat2_f(len_amm, 20,"P_SSC_ia");
  struct MF_results SSC_res, SSC_res_opt;
  SSC_res.Lambda[1]=0; SSC_res_opt.Lambda[1]=0;
  if(SCO>=0){
    for(j=0; j<N_str_mut; j++){
      if(isnan(Str_res[j].score))continue;
      float **SC_mod=Str_mod[j];
      for(int k=0; k<2; k++){
	float **Stab_mod; float ratio;
	if(k==0){
	  if(MF_COMP==0)continue;
	  Stab_mod=MF_mod; ratio=1./Str_res[j].Lambda[0];
	  sprintf(SSC_TYPE, "%sMF", STR_MUT_T[j]);
	}else{
	  Stab_mod=WT_mod; ratio=WT_res.Lambda[0]/Str_res[j].Lambda[0];
	  sprintf(SSC_TYPE, "%sWT", STR_MUT_T[j]);
	}
	if(isnan(ratio)){
	  printf("ERROR in %s, Lambda(%s)= %.3g\n",
		 SSC_TYPE, STR_MUT_T[j],Str_res[j].Lambda[0]);
	  exit(8);
	}
	for(i=0; i<len_amm; i++){
	  for(a=0; a<20; a++){
	    SSC_mod[i][a]=ratio*Stab_mod[i][a]+SC_mod[i][a];
	  }
	}
	printf("Optimizing Lambda for %s model\n", SSC_TYPE);
	Qmax=Normalize_exponent(SSC_mod, len_amm, SSC_TYPE);
	if(Qmax>0){
	  if(Str_res[j].Lambda[0]>0)Lambda_start[0]=Str_res[j].Lambda[0];
	  if(Stab_res->Lambda[0]>0) Lambda_start[1]=Stab_res->Lambda[0];
	  Optimize_distr(&SSC_res, P_SSC_ia, Str_mod[j], Stab_mod, P_mut_a,
			 f_reg_ia, f_msa_ia, wi, &E_wt, C_nat, i_sec,
			 name_file, file_summ, SSC_TYPE, 1);
	  printf("Opt: %.3g %.3g %.3g\n", SSC_res.score,
		 SSC_res.Lambda[0], SSC_res.Lambda[1]);
	  if(isnan(SSC_res.score))continue;
	  if((SSCO<0)||(SSC_res.score>SSC_res_opt.score)){
	    SSCO=0; SSC_res_opt=SSC_res;
	    Stab_mod_opt=Stab_mod; SCO=j;
	    strcpy(SSC_TYPE_OPT, SSC_TYPE);
	    Copy_P(SSC_mod_opt, SSC_mod, len_amm, 20);
	    Copy_P(P_SSC_opt_ia, P_SSC_ia, len_amm, 20);
	  }
	  fprintf(out, "Stability and structure fitness %s combined as f= ",
		  SSC_TYPE);
	  fprintf(out, "f_stab + %.3f f_str\n", ratio);
	}
      }
    }
  }
     
  // Print site specific substitution matrices, frequencies and
  // exchangeabilities
  fprintf(out, "\nComputing and printing site-specific amino-acid ");
  fprintf(out, "frequencies and exchangeability matrices\n");

  fprintf(file_summ,
	  "# The best stability constrained, structure constrained ");
  fprintf(file_summ,"and combined model are:\n");
  
  if(strncmp(STAB_TYPE, "MF", 2)==0){
    fprintf(out, "Mean-field model (MF)\n");
  }else{
    fprintf(out, "Wild-type model (WT). DDG of all possible mutations ");
    fprintf(out, "of wild-type sequence\n");
  }
  fprintf(file_summ, "%s\n", STAB_TYPE);
  Print_profiles(P_Stab_ia, STAB_TYPE,Stab_res->DG,Stab_res->Lambda[0],
		 P_mut_a, aa_seq, len_amm, name_file, PRINT_E, wi, out);
  Print_exchange(P_Stab_ia, STAB_TYPE, res_short, len_amm,
		 P_mut_a, P_cod, Q_cod, tt_ratio, TWONUC,
		 name_file, FORMAT, EXCHANGE, MATRIX, PRINT_E, wi, out);

  if(SCO>=0){
    fprintf(file_summ, "%s\n",STR_MUT_T[SCO]);
    fprintf(file_summ, "%s\n",SSC_TYPE_OPT);
    fprintf(out, "Best structure-constrained model (%s). %s ",
	    STR_MUT_T[SCO],STR_MUT_T[SCO]);
    fprintf(out, "of all possible mutations of wild-type sequence\n");
    Print_profiles(P_Str_ia[SCO], STR_MUT_T[SCO],
		   Str_res[SCO].DG,Str_res[SCO].Lambda[0],
		   P_mut_a,  aa_seq, len_amm, name_file, PRINT_E, wi, out);
    Print_exchange(P_Str_ia[SCO], STR_MUT_T[SCO], res_short, len_amm,
		   P_mut_a, P_cod, Q_cod, tt_ratio, TWONUC,
		   name_file, FORMAT, EXCHANGE, MATRIX, PRINT_E, wi, out);
  }

  // Change stationary frequencies
  if(REMUT && GET_FREQ && SSCO>=0){
    // Change mutation model
    float P_mut_backup[20];
    for(a=0; a<20; a++)P_mut_backup[a]=P_mut_a[a];
    float fmut[20], P_sel[20]; double sum=0;
    Compute_P_sel(P_sel, P_SSC_opt_ia, P_mut_a, len_amm);
    for(a=0; a<20; a++){fmut[a]=num_aa[a]/P_sel[a]; sum+=fmut[a];}
    for(a=0; a<20; a++)fmut[a]/=sum;
    if(GET_FREQ==3){
      for(a=0; a<20; a++)P_mut_a[a]=fmut[a];
    }else{
      Get_mut_par(mut_par, P_mut_a, P_cod, Q_cod, GET_FREQ,
		  fmut, len_amm, num_dna, target.name, 1);
      kCpG=mut_par[4]; tt_ratio=mut_par[5]; TWONUC=mut_par[6];
      Output_name(name_file, dir_out, target.name, TEMP, sU1, sC1, MEANFIELD,
		  MODEL, LAMBDA, OPT_LAMBDA, NEUTRAL, N_pop, mut_par);
      fprintf(out, "New mutation model:\nNucleotide frequencies ");
      for(i=0; i<4; i++)fprintf(out, " %c %.3f", Nuc_code(i), mut_par[i]);
      fprintf(out, " CpG ratio: %.3f", kCpG);
      fprintf(out, " trasition-transversion ratio: %.3f", tt_ratio);
      fprintf(out, " Mutations involving two nucleotides: %.3f\n", TWONUC);
      //sprintf(name_file, "%s_iter%d", name_file, iter+2);
    }
    fprintf(out, "a.a. frequencies previous step: ");
    for(a=0; a<20; a++)fprintf(out," %.3f",P_mut_backup[a]);
    fprintf(out, "\na.a. frequencies current step:  ");
    for(a=0; a<20; a++)fprintf(out," %.3f",P_mut_a[a]);
    fprintf(out,"\n");

    // Compute distribution
    fprintf(file_summ, "# Changing a.a. frequencies\n");
    printf("Optimizing Lambda for %s model\n", SSC_TYPE_OPT);
    for(i=0; i<2; i++){
      if(SSC_res_opt.Lambda[i]>0)Lambda_start[i]=SSC_res_opt.Lambda[i];
    }
    Optimize_distr(&SSC_res,P_SSC_ia, Str_mod[SCO], Stab_mod_opt, P_mut_a,
		   f_reg_ia,f_msa_ia,wi, &E_wt, C_nat, i_sec,
		   name_file, file_summ, SSC_TYPE_OPT, 1);
    printf("Opt: %.3g %.3g %.3g\n", SSC_res.score,
	   SSC_res.Lambda[0], SSC_res.Lambda[1]);
    fprintf(file_summ, "# New a.a. frequencies ");
    if(SSC_res.score>SSC_res_opt.score){
      SSC_res_opt=SSC_res;
      Copy_P(P_SSC_opt_ia, P_SSC_ia, len_amm, 20);
      fprintf(file_summ, "improve the score\n");
      int renorm=0; float tiny=0.01;
      for(a=0; a<20; a++)if(P_mut_a[a]<tiny){P_mut_a[a]=tiny; renorm=1;}
      if(renorm){
	float sum=0; for(a=0; a<20; a++)sum+=P_mut_a[a];
	for(a=0; a<20; a++)P_mut_a[a]/=sum;
      }
    }else{
      fprintf(file_summ, "do not improve the score\n");
      for(a=0; a<20; a++)P_mut_a[a]=P_mut_backup[a];
    }
  }
  // Optimal regularization, maximizing specific heat
  if(SSCO>=0 && OPT_REG){
    for(int n=0; n<2; n++){
      if(SSC_res_opt.Lambda[n]>0)Lambda_start[n]=SSC_res_opt.Lambda[n];
    }
    Optimize_reg(&SSC_res_opt, P_SSC_opt_ia, reg_ini, w_max, f_aa,
		 &SSC_res,P_SSC_ia, Str_mod[SCO], Stab_mod_opt, P_mut_a,
		 f_reg_ia,f_msa_ia,wi, &E_wt, C_nat, i_sec, name_file,
		 file_summ, SSC_TYPE_OPT);
  }
     

  if(SSCO>=0){
    fprintf(file_summ, "%s\n", SSC_TYPE_OPT);
    fprintf(out, "Structure and stability constrained model (%s). %s ",
	    SSC_TYPE_OPT,SSC_TYPE_OPT);
    fprintf(out, "of all possible mutations of wild-type sequence\n");
    Print_profiles(P_SSC_opt_ia,SSC_TYPE_OPT,SSC_res_opt.DG,
		   SSC_res_opt.Lambda[0], P_mut_a,
		   aa_seq, len_amm, name_file, PRINT_E, wi, out);
    Print_exchange(P_SSC_opt_ia, SSC_TYPE_OPT, res_short, len_amm,
		   P_mut_a, P_cod, Q_cod, tt_ratio, TWONUC,
		   name_file, FORMAT, EXCHANGE, MATRIX, PRINT_E, wi, out);
  }

  fclose(file_summ);

 Compute_all_muts:
  if(ALL_MUTS && idna){
    char name_mut[400];
    sprintf(name_mut, "%s_all_nuc_mutations.dat", file_seq);
    FILE *file_mut=fopen(name_mut, "w");
    Compute_nuc_mut(dna_seq, len_dna, aa_seq, len_amm, codon, coded_aa,
		    STAB_TYPE, Stab_mod_opt, NULL, Stab_res->Lambda, file_mut);
    if(SCO>=0)
      Compute_nuc_mut(dna_seq, len_dna, aa_seq, len_amm, codon, coded_aa,
		      STR_MUT_T[SCO], Str_mod[SCO], NULL, Str_res[SCO].Lambda,
		      file_mut);
    if(SSCO>=0)
      Compute_nuc_mut(dna_seq, len_dna, aa_seq, len_amm, codon, coded_aa,
		      SSC_TYPE_OPT, Str_mod[SCO], Stab_mod_opt,
		      SSC_res_opt.Lambda, file_mut);
  }

  /******************** End Mean-field  *********************/

 Simulate:
  if(IT_MAX==0)return(0);
  
  /***********************************************************

                      Simulations of evolution
 
  *************************************************************/
  // S samples are simulated each for IT_MAX iterations
  fprintf(out, "Simulations: %d samples of %d substitutions each\n",
	  Samples, IT_MAX);
  char fit_def[100];
  if(NEUTRAL==0){
    sprintf(fit_def, "Fitness=1./(1+exp(DeltaG)), Population size= %d\n",N_pop);
  }else{
    sprintf(fit_def,"Neutral fitness=Theta(%.2f*DeltaG_PDB-DeltaG)\n",
	    DG_THR_COEFF);
  }
  fprintf(out, "%s\n", fit_def);
  fprintf(out, "Mutations are simulated at the DNA level ");
  fprintf(out, "using the genetic code %s\n", FILE_CODE);

  char name_simul[200]; sprintf(name_simul, "%s_Simul", target.name);
  FILE *file_ave =open_file(name_simul, EXT_AVE, aa_seq, 0, fit_def);
  FILE *file_stab=open_file(name_simul, EXT_OUT, aa_seq, 1, fit_def);

  // Accumulate samples
  double p_accept_all=0, p2_accept_all=0;
  double p_stab_incr_all=0, p2_stab_incr_all=0;

  double fit1_all=0, fit2_all=0, DG1_all=0, DG2_all=0,
    Enat1_all=0, Enat2_all=0, dNdS1_all=0, dNdS2_all=0,
    accept1_all=0, accept2_all=0, seqentr1_all=0, seqentr2_all=0;
  long it_all=0;
  struct load mut_load_all, trans_load_all;
  Initialize_load(&mut_load_all);
  Initialize_load(&trans_load_all);

  // Global variables valid for all independent samples
  // Number of iterations
  int it_print=10; if(it_print > IT_MAX*0.1)it_print= IT_MAX*0.1;
  int it_trans=it_print;
  int NPRINT= 1; if(it_print)NPRINT+=(IT_MAX/it_print);
  int MUTMAX= len_amm;  // Exhaustive search after MUTMAX unfixed mutations  

  // Amino acid distributions and entropy
  // Reference sequence
  short *aa_seq0=malloc(len_amm*sizeof(short));
  for(i=0; i<len_amm; i++)aa_seq0[i]=aa_seq[i];
  double **aa_distr=malloc(len_amm*sizeof(double *));
  double **aa_distr0=malloc(len_amm*sizeof(double *));
  double **aa_distr_all=malloc(len_amm*sizeof(double *));
  for(i=0; i<len_amm; i++){
     aa_distr[i]=malloc(20*sizeof(double));
     for(j=0; j<20; j++)aa_distr[i][j]=0;
     aa_distr0[i]=malloc(20*sizeof(double));
     for(j=0; j<20; j++)aa_distr0[i][j]=0;
     aa_distr_all[i]=malloc(20*sizeof(double));
     for(j=0; j<20; j++)aa_distr_all[i][j]=0;
  }
  float seq_entr_mut=Sequence_entropy_mut(mut_par, codon, coded_aa);

  // Nucleotide distributions at each codon position
  long **nuc_evo=malloc(len_dna*sizeof(long *));
  for(i=0; i<len_dna; i++){
    nuc_evo[i]=malloc(4*sizeof(long));
    for(j=0; j<4; j++)nuc_evo[i][j]=0;
  }

  // Averages
  float *fit_evo=malloc(NPRINT*sizeof(float));
  float *DG_evo=malloc(NPRINT*sizeof(float));
  float *Enat_evo=malloc(NPRINT*sizeof(float));

  // Thermodynamic computations
  struct REM E_mut;
  Initialize_E_REM(&E_mut, E_wt.L, E_wt.REM,
		   E_wt.T, E_wt.S_C, E_wt.S_U, FILE_STR, 0);

  // Fitness
  if(MEANFIELD)Divide_by_Pmut(P_MF_ia, P_mut_a, len_amm, 20);
  int EXHAUSTIVE=0;
  if((MEANFIELD==0)&&(N_pop > len_amm*10))EXHAUSTIVE=1;
  int INI_EXH=EXHAUSTIVE;
  float Inverse_pop=1./N_pop;

  /***********************
         MUTANT
  ************************/
  double fitness_mut;
  char dna_new;
  int nuc_mut, nuc_new, res_mut=0, aa_new;

  /***********************
          OUTPUT
  ************************/

  // FILE *file_dna=open_file(name_simul, EXT_DNA, aa_seq, 2, fit_def);

  FILE *file_msa=NULL; char name_MSA[300];
  if(PRINT_MSA==0 || IT_MAX==0){
    MAX_MSA=0;
  }else{
    sprintf(name_MSA, "%s%s", name_simul, EXT_MSA);
    file_msa=fopen(name_MSA, "w");
    Print_seq(file_msa, aa_seq, len_amm, &nseq_msa, E_wt.DeltaG);
    fprintf(out,"MSA from simulation printed in %s\n", name_MSA);
  }
  char name_div[300]; FILE *file_div;
  if(PRINT_TN){
    sprintf(name_div, "%s_TN_div.dat", name_simul);
    file_div=fopen(name_div, "a");
    fprintf(file_div, "# Protein %s L=%d\n", target.name, len_amm);
    fprintf(file_div, "#Tajima-Nei_divergence num_subst\n");
    fprintf(out,"Writing Tajima-Nei divergence versus time in %s\n",name_div);
  }
  sprintf(dumm, "%s%s", name_simul, EXT_FIN);
  fprintf(out,"Final results of evolutionary simulations printed in %s\n",dumm);
  sprintf(dumm, "%s%s", name_simul, EXT_SEL);
  fprintf(out, "Selection summary printed in %s\n", dumm);
  sprintf(dumm, "%s%s", name_simul, EXT_AVE);
  fprintf(out, "Average quantities obtained in simulations printed in %s\n",
	  dumm);
  sprintf(dumm, "%s%s", name_simul, EXT_OUT);
  fprintf(out, "Results for every accepted substitution printed in %s\n",dumm);


  /****************************************************
                 S samples of evolution
  *****************************************************/

  for(int is=0; is<Samples; is++){

    // Start the trajectory

    // Number of substitutions
    int aa_subst=0, synonymous;
    int it1=0;
    long it_subst=0; //int msa_subst=0;
    // Mutations per one aa substitution
    long tot_mut=0, naa_mut=0, nsyn_mut=0, nsyn_subst=0;

    // Count substitutions
    long num_syn_subst=0, num_aa_subst=0;
    long num_syn_mut=0, num_aa_mut=0;
    int num_stab_incr=0;

    // Average stability and fitness
    long it_sum=0;
    double E_nat_ave=0, f_ave=0, DG_ave=0;
    double E_nat_dev=0, f_dev=0, DG_dev=0;

    // Sequence entropy
    double seq_entr_sum=0, seq_entr_dev=0, entr_dev=0;
    float seq_entr;
    int n_seq_entr=0;

    for(i=0; i<len_amm; i++)aa_seq[i]=aa_seq0[i];
    for(i=0; i<len_amm; i++){
      for(j=0; j<20; j++)aa_distr[i][j]=0;
    }

    // Loads
    struct load mut_load, trans_load;
    Initialize_load(&mut_load);
    Initialize_load(&trans_load);

    // Wild-type stability
    E_wt.DeltaG=
      Compute_DG_overT_contfreq(&E_wt, aa_seq, C_nat, i_sec, 0);    
    // Wild-type fitness
    if(NEUTRAL==0){
      fitness_wt=1./(1+exp(E_wt.DeltaG));
    }else{
      fitness_wt=1;
      DG_thr=E_wt.DeltaG*DG_THR_COEFF;
    }
    fprintf(file_stab, "#\n#\n# Evolutionary trajectory %d\n#\n#\n",is+1);
    fprintf(file_stab, "%3d  %c%c  %.3f  %.3f %7.4f",
	    0, AMIN_CODE[aa_seq[0]], AMIN_CODE[aa_seq[0]],
	    E_wt.E_nat, E_wt.DeltaG, fitness_wt);

    // Record fitness, energy and DeltaG
    int nsam=0;
    Record_evo(fit_evo, fitness_wt, DG_evo, E_wt.DeltaG,
	       Enat_evo, E_wt.E_nat, &nsam);
    
    // Mutation rates
    Ini_count(dna_seq, len_dna, count);
    Compute_rates(rate, mut_par, tt_ratio);


    // Simulations
    while(1){
    
      if(EXHAUSTIVE){
	// Extract next substitution (requires full exploration of sequence
	// neighbors, only convenient if N is large).
	Substitute_exhaustive(&res_mut, &aa_new, &nuc_mut, &nuc_new,
			      &mut_load, &trans_load, it_subst-it_trans,
			      &fitness_mut, &E_mut, &E_wt, 
			      &naa_mut, &nsyn_mut, &nsyn_subst,
			      NEUTRAL, DG_thr, N_pop, C_nat, i_sec,
			      aa_seq, dna_seq, dna_seq_short, len_dna,
			      codon, coded_aa, tt_ratio, mut_par);
	tot_mut=naa_mut+nsyn_subst+nsyn_mut+1;
	dna_new=Nuc_code(nuc_new);
	if(it_subst > it_trans){
	  num_aa_mut += naa_mut;
	  num_syn_mut+= nsyn_mut;
	  num_syn_subst += nsyn_subst;
	}
	goto Update_aa;
      }

      tot_mut++;
      if(tot_mut>=MUTMAX){
	printf("WARNING, too many unfixed mutations > %d\n", MUTMAX);
	printf("T= %.2f MEANFIELD= %d NEUTRAL= %d", TEMP,MEANFIELD,NEUTRAL); 
	if(NEUTRAL==0)printf(" N= %d",N_pop);
	printf(" subst= %ld transient=%d\n", it_subst, it_trans);
	printf("DeltaG = %.3g ",E_wt.DeltaG);
	E_wt.DeltaG=Compute_DG_overT_contfreq(&E_wt, aa_seq, C_nat, i_sec, 0);
	printf("DeltaG recomputed = %.3g\n",E_wt.DeltaG);
	if(MEANFIELD==0){
	  printf("Performing exhaustive search of mutants\n");
	  EXHAUSTIVE=1; INI_EXH=1;
	  aa_subst=1; synonymous=0;
	  continue;
	}else{
	  exit(8);
	}
      }

      // Individual mutation
      synonymous=Mutate_seq(dna_seq, len_dna, codon, coded_aa,
			    aa_seq, len_amm, mut_par, tt_ratio, count, rate,
			    &nuc_mut, &dna_new, &res_mut, &aa_new);

      if(synonymous > 0){
	nsyn_mut++;
	if(it_subst >= it_trans)num_syn_mut++; 
	//if((MEANFIELD==0)&&(RandomFloating() > Inverse_pop))continue;
	if((RandomFloating() > Inverse_pop))continue;
	nsyn_subst++;
	if(it_subst >= it_trans)num_syn_subst++;
	nuc_new=Code_nuc(dna_new);
	goto Update_dna;

      }else if(synonymous==0){

	if(it_subst >= it_trans)num_aa_mut++;

	// Compute stability and fitness
	Copy_E_REM(&E_mut, &E_wt);
	E_mut.DeltaG=
	  Mutate_DG_overT_contfreq(&E_mut,aa_seq,C_nat,i_sec,res_mut,aa_new);
	if(0){
	  int aa_old=aa_seq[res_mut]; aa_seq[res_mut]=aa_new;
	  struct REM E_mut2;
	  Initialize_E_REM(&E_mut2,E_wt.L, E_wt.REM,
			   E_wt.T, E_wt.S_C, E_wt.S_U, FILE_STR, 0);
	  Copy_E_REM(&E_mut2, &E_wt);
	  float DeltaG=
	    Compute_DG_overT_contfreq(&E_mut2, aa_seq, C_nat, i_sec, 1);
	  aa_seq[res_mut]=aa_old;
	  if(fabs(DeltaG-E_mut.DeltaG)>1){
	    printf("WARNING, DG= %.4g (full) %.4g (increment)\n",
		   DeltaG, E_mut.DeltaG);
	    printf("DEnat= %.3g DE1= %.3g DE2= %.3g "
		   "DE21= %.3g DE22= %.3g DE2s1= %.3g DE2s2= %.3g DE3= %.3g ",
		   E_mut2.E_nat-E_mut.E_nat, E_mut2.E1-E_mut.E1,
		   E_mut2.E2-E_mut.E2,
		   E_mut2.E2cont1-E_mut.E2cont1,
		   E_mut2.E2cont2-E_mut.E2cont2,
		   E_mut2.E2site1-E_mut.E2site1,
		   E_mut2.E2site2-E_mut.E2site2,
		   E_mut2.E3-E_mut.E3);
	    double sum=0; int i;
	    for(i=0; i<E_wt.L; i++)sum+=fabs(E_mut2.c1U1[i]-E_mut.c1U1[i]);
	    printf(" sum|DE1_i|= %.3g\n", sum);
	    E_mut.DeltaG=DeltaG;
	  }
	}
	if(NEUTRAL){
	  if(E_mut.DeltaG<DG_thr){fitness_mut=1;}else{fitness_mut=0;}
	  aa_subst=fitness_mut;
	}else{
	  fitness_mut=1./(1+exp(E_mut.DeltaG));
	  aa_subst=Selection(fitness_mut, fitness_wt, N_pop);
	}
	if(aa_subst<=0)continue;

      }else{ 
	if(it_subst >= it_trans)num_aa_mut++;
	continue;  // Stop codon
      }

    Update_aa:
      // A non-synonymous substitution has occurred

      // Print thermodynamic properties (also in transient)
      it_subst++;
      fprintf(file_stab, " %2ld %3ld\n", nsyn_subst, tot_mut);
      if(INI_EXH){
	fprintf(file_stab, "# Exhaustive search\n"); INI_EXH=0;
      }
      fprintf(file_stab, "%3d  %c%c  %.3f  %.3f %7.4f",
	      res_mut, AMIN_CODE[aa_seq[res_mut]], AMIN_CODE[aa_new],
	      E_mut.E_nat, E_mut.DeltaG, fitness_mut);
      fflush(file_stab);

      if((it_subst/it_print)*it_print == it_subst){
	Record_evo(fit_evo, fitness_wt, DG_evo, E_mut.DeltaG,
		   Enat_evo, E_mut.E_nat, &nsam);
      }
    
      // Write MSA 
      if(nseq_msa<MAX_MSA ){ //&& it_subst-msa_subst>5){
	//msa_subst=it_subst;
	Print_seq(file_msa, aa_seq, len_amm, &nseq_msa, E_mut.DeltaG);
      }
      
      // Statistics after the transient
      if(it_subst >= it_trans){

	// Average old amino acid sequence, weight=tot_mut
      
	// Update amino acid distribution
	for(i=0; i<len_amm; i++)aa_distr0[i][aa_seq[i]]+=tot_mut;
      
	// Averages
	E_nat_ave +=tot_mut*E_wt.E_nat;
	E_nat_dev+=tot_mut*E_wt.E_nat*E_wt.E_nat;
	DG_ave+=tot_mut*E_wt.DeltaG;
	DG_dev+=tot_mut*E_wt.DeltaG*E_wt.DeltaG;
	f_ave+=tot_mut*fitness_wt;
	f_dev+=tot_mut*fitness_wt*fitness_wt;
	it_sum+=tot_mut;
     
	num_aa_subst++; it1++;
	if(E_mut.DeltaG<E_wt.DeltaG)num_stab_incr++;

	// Print averages
	if(it1==it_print){
	  it1=0;
	  seq_entr=Sequence_entropy(aa_distr0, len_amm)-seq_entr_mut;
	  seq_entr_sum+=seq_entr; seq_entr_dev+=seq_entr*seq_entr;
	  n_seq_entr++;
	  for(i=0; i<len_amm; i++){
	    double *aa0=aa_distr0[i];
	    for(j=0; j<20; j++){aa_distr[i][j]+=aa0[j]; aa0[j]=0;}
	  }
	  Print_ave(file_ave, it_sum, num_aa_subst, N_pop,
		    f_ave, f_dev, E_nat_ave, E_nat_dev, DG_ave, DG_dev,
		    num_syn_subst, num_aa_subst, num_syn_mut, num_aa_mut,
		    seq_entr, mut_load, trans_load);
	  if(PRINT_TN){
	    aa_seq[res_mut]=aa_new;
	    Print_TN_div(aa_seq, aa_seq0, len_amm, it_subst, file_div);
	  }
	    
	} // end print
      }

      // Update_AA:
      aa_seq[res_mut]=aa_new;
      Copy_E_REM(&E_wt, &E_mut);
      fitness_wt=fitness_mut;
      tot_mut=0; nsyn_subst=0;

    Update_dna:
      if(aa_subst ||(synonymous > 0)){
	count[nuc_new]++;
	count[dna_seq_short[nuc_mut]]--;
	for(i=0; i<len_dna; i++)nuc_evo[i][dna_seq_short[i]]+=nsyn_mut;
	nsyn_mut=0;
	dna_seq_short[nuc_mut]=nuc_new;
	dna_seq[nuc_mut]=dna_new;
      }

      if(num_aa_subst == IT_MAX)break;
      
    }
    fprintf(file_stab, " %2ld %3ld\n", nsyn_subst, tot_mut);

    /*********************  End of simulation  **************************/

    // Loads
    /*Compute_load(&Tload_sum, &Tload_dev, &Mload_sum, &Mload_dev, &Nload,
      E_wt, C_nat, i_sec, aa_seq, fitness_wt, dna_seq, len_dna,
      codon, coded_aa);*/

    // Entropy
    for(i=0; i<len_amm; i++){
      double *aa0=aa_distr0[i];
      for(j=0; j<20; j++){aa_distr[i][j]+=aa0[j]; aa0[j]=0;}
    }
    seq_entr=Sequence_entropy(aa_distr, len_amm)-seq_entr_mut;
    for(i=0; i<len_amm; i++){
      double *aa1=aa_distr[i];
      for(j=0; j<20; j++){aa_distr_all[i][j]+=aa1[j]; aa1[j]=0;}
    }

    if(n_seq_entr > 1){
      entr_dev=seq_entr_dev-seq_entr_sum*seq_entr_sum/n_seq_entr;
      entr_dev=sqrt(entr_dev)/n_seq_entr;
    }
    
    it_sum+=tot_mut;
    float dN_dS, accept;
    Print_final(name_simul, it_sum, TEMP, sU1, sC1, sC0, MEANFIELD, LAMBDA,
		N_pop, mut_par, tt_ratio,
		fit_evo, DG_evo, Enat_evo, nsam, it_print,
		&f_ave, f_dev, &E_nat_ave, E_nat_dev, &DG_ave, DG_dev,
		seq_entr, entr_dev,
		&mut_load, &trans_load,
		num_syn_subst, num_aa_subst, num_syn_mut, num_aa_mut,
		&dN_dS, &accept, nuc_evo, len_dna, is);

    // Average samples
    it_all+=it_sum;
    fit1_all+=f_ave; fit2_all+=f_ave*f_ave;
    DG1_all+=DG_ave; DG2_all+=DG_ave*DG_ave;
    Enat1_all+=E_nat_ave; Enat2_all+=E_nat_ave*E_nat_ave;
    Sum_loads(&trans_load_all, &trans_load);
    Sum_loads(&mut_load_all, &mut_load);
    dNdS1_all+=dN_dS; dNdS2_all+=dN_dS*dN_dS;
    accept1_all+=accept; accept2_all+=accept*accept;
    seqentr1_all+=seq_entr; seqentr2_all+=seq_entr*seq_entr;

    {
      float p=num_aa_subst/(float)num_aa_mut;
      p_accept_all+=p; p2_accept_all+=p*p;
      p=num_stab_incr/(float)num_aa_subst;
      p_stab_incr_all+=p; p2_stab_incr_all+=p*p;
    }
  }

  /***************************************************
                     End of samples
  ***************************************************/

  if(MAX_MSA){
    printf("MSA printed in %s\n", name_MSA); fclose(file_msa);
  }

  {// Print summary on selection
    char name_sel[300];
    sprintf(name_sel, "%s%s", name_simul, EXT_SEL);
    FILE *file_sel=fopen(name_sel, "w");
    fprintf(out, "\nSimulating %d samples of evolutionary trajectories with %d substitution each and printing accepted mutations and fraction of substitutions with increased stability in file %s\n", Samples, IT_MAX, name_sel);
    float p=p_accept_all/Samples, var;
    fprintf(file_sel,
	    "# averages based on %d samples of %d substitutions each\n",
	    Samples, IT_MAX);
    if(Samples>1){var=(p2_accept_all-Samples*p*p)/(Samples-1);}
    else{var=p*p;}
    fprintf(file_sel, "Accepted mutations: %.3g s.e.= %.2g\n",
	    p, sqrt(var));
    p=p_stab_incr_all/Samples;
    if(Samples>1){var=(p2_stab_incr_all-Samples*p*p)/(Samples-1);}
    else{var=p*p;}
    fprintf(file_sel,
	    "Selected substitutions with increased stability: "
	    "%.3g s.e.= %.2g\n", p, sqrt(var));
    fclose(file_sel);
  }
    
  FILE *file_out=open_file(name_simul,"_samples.dat",aa_seq,0, fit_def);
  char name_sam[240]; sprintf(name_sam, "%s_samples.dat", name_simul);
  fprintf(out, "Printing averages for each sample (DeltaG, E_nat, fitness, "
	  "mutation load, translation load, dN/dS, acceptance ratio, "
	  "sequence entropy) in file %s\n", name_sam);

  fprintf(file_out, "# %d samples\n", Samples);
  fprintf(file_out, "# what mean s.e.\n");
  double x, dx; int S=Samples;
  Get_mean(&x, &dx, DG1_all, DG2_all, S, S); DG1_all=x;
  fprintf(file_out, "DeltaG/T\t%.5g\t%.2g\n", x, dx);
  fprintf(file_out, "DeltaG\t%.5g\t%.2g\n", x*TEMP, dx*TEMP);
  Get_mean(&x, &dx, Enat1_all, Enat2_all, S, S);
  fprintf(file_out, "E_nat\t%.5g\t%.2g\n", x, dx);
  Get_mean(&x, &dx, fit1_all, fit2_all, S, S);
  fprintf(file_out, "Fitness\t%.5g\t%.2g\n", x, dx);

  Get_mean(&x, &dx,mut_load_all.df_ave, mut_load_all.df_dev, S, S);
  fprintf(file_out, "Mut.load.fitness\t%.5g\t%.2g\n", x, dx);
  Get_mean(&x, &dx, mut_load_all.dG_ave, mut_load_all.dG_dev, S, S);
  fprintf(file_out, "Mut.load.DeltaG\t%.5g\t%.2g\n", x, dx);

  Get_mean(&x, &dx, trans_load_all.df_ave, trans_load_all.df_dev, S, S);
  fprintf(file_out, "Trans.load.fitness\t%.5g\t%.2g\n", x, dx);
  Get_mean(&x, &dx, trans_load_all.dG_ave, trans_load_all.dG_dev, S, S);
  fprintf(file_out, "Trans.load.DeltaG\t%.5g\t%.2g\n", x, dx);

  Get_mean(&x, &dx, dNdS1_all, dNdS2_all, Samples, Samples);
  fprintf(file_out, "dN/dS\t%.5g\t%.2g\n", x, dx);
  Get_mean(&x, &dx, accept1_all, accept2_all, Samples, Samples);
  fprintf(file_out, "acceptance\t%.5g\t%.2g\n", x, dx);
  Get_mean(&x, &dx, seqentr1_all, seqentr2_all, Samples, Samples);
  fprintf(file_out, "Seq.entropy\t%.5g\t%.2g\n", x, dx);
  fclose(file_out);
  Print_profile_evo(name_simul,aa_distr_all,aa_seq0,len_amm,DG1_all,it_all);
  sprintf(dumm, "%s_AA_profiles_evo.txt", name_simul);
  fprintf(out, "Printing evolutionary profiles in file %s\n", dumm);

  printf("Writing output files and explanations in %s\n", out_file);
  fclose(out);
  
  // Free memory
  if(aa_seq)free(aa_seq);
  if(aa_seq0)free(aa_seq0);
  if(aa_distr){for(i=0; i<len_amm; i++)free(aa_distr[i]); free(aa_distr);}
  if(aa_distr0){for(i=0;i<len_amm; i++)free(aa_distr0[i]);free(aa_distr0);}
  if(dna_seq)free(dna_seq);
  if(dna_seq_short)free(dna_seq_short);
  if(nuc_evo)free(nuc_evo);
  Empty_E_REM(&E_wt);
  Empty_E_REM(&E_mut);
  return(0);
}


void Print_ave(FILE *file_ave,
	       long it_sum, long n_subst,
	       int N_pop,
	       double f_sum, double f_dev,
	       double E_sum, double E_dev,
	       double DG_sum, double DG_dev,
	       long num_syn_subst, long num_aa_subst,
	       long num_syn_mut, long num_aa_mut,
	       float seq_entr,
	       struct load mut_load, struct load trans_load)
{
//   float t_indip=n_subst/20;
   float t_indep=1.0;

  if(ini_print==0){
    fprintf(file_ave, "# Fitness (sd)     Enat  (sd)    DG  (sd)");
    fprintf(file_ave, " seq.entropy(mut)-seq_entropy(sel)     ");
    //fprintf(file_ave, " Trans.load   (sd)  Mut.load  (sd)");
    fprintf(file_ave, "  nonsyn/syn accept  N_subst\n");
    ini_print=1;
  }

  double x, dx;
  Get_mean(&x, &dx, f_sum, f_dev, it_sum, t_indep);
  fprintf(file_ave, " %.4g\t%.2g\t", x, dx);
  Get_mean(&x, &dx, E_sum, E_dev, it_sum, t_indep);
  fprintf(file_ave, " %.4g\t%.2g\t", x, dx);
  Get_mean(&x, &dx, DG_sum, DG_dev, it_sum, t_indep);
  fprintf(file_ave, " %.4g\t%.2g\t", x, dx);
  fprintf(file_ave, " %.4f\t", -seq_entr);

  fprintf(file_ave, " %.3f\t",
	  (float)(num_aa_subst*num_syn_mut)/(float)(num_syn_subst*num_aa_mut));
  fprintf(file_ave, " %.4g\t", (float)(num_syn_subst+num_aa_subst)/it_sum);
  fprintf(file_ave, " %d\n", (int)num_aa_subst);
  fflush(file_ave);
}



void Get_mean(double *x, double *dx, float sum, float dev,
	      long it_sum, float t_indep)
{
  *x=sum/it_sum;
  double x2=dev-it_sum*(*x)*(*x);
  if(dev<=0){*dx=0; return;}
  *dx=sqrt(x2/(it_sum*t_indep));
}
    
void Print_final(char *name_file, long it_sum,
		 float TEMP, float sU1, float sC1, float sC0,
		 int MEANFIELD, float Lambda, int N_pop,
		 float *mut_par, float tt_ratio,
		 //
		 float *fit_evo, float *DG_evo, 
		 float *Enat_evo, int nsam, int step,
		 double *f_ave, double f_dev, 
		 double *E_ave, double E_dev,
		 double *DG_ave, double DG_dev,
		 float seq_entr, double seq_entr_dev,
		 struct load *mut_load, struct load *trans_load,
		 long num_syn_subst, long num_aa_subst,
		 long num_syn_mut, long num_aa_mut,
		 float *dN_dS, float *accept,
		 long **nuc_evo, int len_dna, int sample)
{
  char name_out[200];
  sprintf(name_out, "%s%s", name_file, EXT_FIN);
  printf("Writing %s\n", name_out);
  FILE *file_out=NULL;

  if(sample==0){
    file_out=fopen(name_out, "w");
    int L=len_dna/3;
    // Headers   
    fprintf(file_out, "# Input thermodynamic parameters:\n");
    fprintf(file_out, "L= %d residues\n", L);
    fprintf(file_out, "TEMP=\t%.2f\n", TEMP);
    fprintf(file_out, "Entropy unfolded=\t%.1f (%.2f *L)\n", sU1*L, sU1);
    fprintf(file_out, "Entropy compact=\t%.1f (%.2f + %.2f *L)\n",
	    sC0+sC1*L, sC0, sC1);
    if(SEC_STR)
      fprintf(file_out, "#Local interactions used, factor= %.3f\n", SEC_STR);
    fprintf(file_out, "# Input population parameters:\n");
    if(MEANFIELD){
      fprintf(file_out, "Meanfield model, Lambda=\t%.2f\n", LAMBDA);
    }else{
      fprintf(file_out, "Population model, Npop=\t%d\n", N_pop);
    }
    fprintf(file_out, "# Input mutation parameters:\n");
    for(int i=0; i<4; i++)
      fprintf(file_out,"%c %.3f ",NUC_CODE[i],mut_par[i]);
    fprintf(file_out, " TT_ratio= %.2f\n", tt_ratio);
  }else{
    file_out=fopen(name_out, "a");
  }
  fprintf(file_out, "#\n#\n# sample %d ", sample+1);
  fprintf(file_out, "Attempted mutations= %ld\n", it_sum);
  fprintf(file_out, "# Results:\n");

  double x, dx, tau, r;  float t_indep=num_aa_subst/10;
  Get_mean(&x, &dx, *f_ave, f_dev, it_sum, t_indep);
  fprintf(file_out, "Fitness ave=\t%.4g %.4g\n", x, dx);
  *f_ave=x;

  x=Logistic_fit(&tau, &r, fit_evo, step, nsam);
  fprintf(file_out, "Fitness, logistic fit= %.4g tau= %.4g r=%.2g\n",x,tau,r);

  Get_mean(&x, &dx, *E_ave, E_dev, it_sum, t_indep);
  fprintf(file_out, "E_nat ave=\t%.4g %.2g\n", x, dx);
  *E_ave=x;

  x=Logistic_fit(&tau, &r, Enat_evo, step, nsam);
  fprintf(file_out, "E_nat, logistic fit= %.4g tau= %.4g r=%.2g\n",x,tau,r);

  Get_mean(&x, &dx, *DG_ave, DG_dev, it_sum, t_indep);
  fprintf(file_out, "DeltaG ave=\t%.4g %.2g\n", x, dx);
  *DG_ave=x;

  x=Logistic_fit(&tau, &r, DG_evo, step, nsam);
  fprintf(file_out, "DeltaG, logistic fit= %.4g tau= %.4g r=%.2g\n",x,tau,r);

  // Synonymous, mutation load
  *dN_dS=(float)(num_aa_subst*num_syn_mut)/(float)(num_syn_subst*num_aa_mut);
  fprintf(file_out, "dN/dS=\t%.3f\n", *dN_dS);

  *accept=(float)(num_syn_subst+num_aa_subst)/it_sum;
  fprintf(file_out, "Acceptance rate=\t%.4g\n", *accept);

  fprintf(file_out, "Seq_entropy(sel)-Seq_entropy(mut)=\t%7.4g %7.4g\n",
	  seq_entr, seq_entr_dev);

  int num=mut_load->num, indep=num/30;
  Get_mean(&x, &dx, mut_load->df_ave, mut_load->df_dev, num, indep);
  fprintf(file_out, "Mut.load.fitness=\t%.5g\t%.2g\n", x, dx);
  mut_load->df_ave=x;
  Get_mean(&x, &dx, mut_load->dG_ave, mut_load->dG_dev, num, indep);
  fprintf(file_out, "Mut.load.DeltaG=\t%.5g\t%.2g\n", x, dx);
  mut_load->dG_ave=x;

  Get_mean(&x, &dx, trans_load->df_ave, trans_load->df_dev, num, indep);
  fprintf(file_out, "Trans.load.fitness=\t%.5g\t%.2g\n", x, dx);
  trans_load->df_ave=x;
  Get_mean(&x, &dx, trans_load->dG_ave, trans_load->dG_dev, num, indep);
  fprintf(file_out, "Trans.load.DeltaG=\t%.5g\t%.2g\n", x, dx);
  trans_load->dG_ave=x;

  // Nucleotide content
  fprintf(file_out, "#\n# Final nucleotide frequencies (1, 2, 3)\n");
  int i, j, n; long nuc_count[4][3];
  for(n=0; n<4; n++){
    for(j=0; j<3; j++)nuc_count[n][j]=0;
  }
  j=0;
  for(i=0; i<len_dna; i++){
    for(n=0; n<4; n++)nuc_count[n][j]+=nuc_evo[i][n];
    j++; if(j==3)j=0;
  }
  double sum=0;
  for(n=0; n<4; n++)sum+=nuc_count[n][0];
  for(n=0; n<4; n++){
    fprintf(file_out, "%c ", NUC_CODE[n]);
    for(j=0; j<3; j++){
      fprintf(file_out," %.3f", nuc_count[n][j]/sum);
    }
    fprintf(file_out, "\n");
  }
  fclose(file_out);
}


int Read_ene_new(char *file_ene, float **interactions)
{
  FILE *file_in=fopen(file_ene,"r");
  int i, j, n=0; float ene;
  char string[200], aa1[8], aa2[8];

  if(file_in==NULL){
    printf("ERROR, energy parameter file %s not found\n", file_ene);
    exit(8);
  }
  while(fgets(string, sizeof(string), file_in)!=NULL){
    sscanf(string, "%s%s%f", aa1, aa2, &ene); n++;
    i=Code_AA(aa1[0]); j=Code_AA(aa2[0]);
    interactions[i][j]=ene; interactions[j][i]=ene;
  }
  return(n);
}


void Read_ene_par(char *file_ene, float **interactions)
{
  FILE *file_in=fopen(file_ene,"r");
  int i; char string[200];
  if(file_in==NULL){
    printf("ERROR, energy parameter file %s not found\n", file_ene);
    exit(8);
  }
  
  for(i=0; i<20; i ++){
    float *MAT=interactions[i];
    fgets(string, sizeof(string), file_in);
    sscanf(string, 
	   "%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f%f", 
	   &MAT[0],&MAT[1],&MAT[2],&MAT[3],&MAT[4],
	   &MAT[5],&MAT[6],&MAT[7],&MAT[8],&MAT[9],&MAT[10],
	   &MAT[11],&MAT[12],&MAT[13],&MAT[14],&MAT[15],
	   &MAT[16],&MAT[17],&MAT[18],&MAT[19]);
  }
  /*fgets(string, sizeof(string), matrix); // Disulfide bonds
    sscanf(string, "%f", &interactions[210]);
    interactions[210]-=interactions[label[17][17]];*/
  (void)fclose(file_in);
  return;
}

char *Read_sequence(int *len_dna, int *nseq, int **ini_seq, int **len_seq,
		    char *inputseq)
{  
  char *sequence, string[1000], *ptr, tmp[80];
  FILE *file_in=fopen(inputseq, "r");
  int i=0;

  if(file_in==NULL){
    printf("WARNING, sequence file %s does not exist\n", inputseq);
    return(NULL);
  }

  *nseq=0;
  printf("Reading %s\n",inputseq);
  strcpy(seq_name,"");
  while(fgets(string, sizeof(string), file_in)!=NULL){
    if(string[0]=='>'){
      for(i=0; i<sizeof(string); i++)
	if(string[i]=='\n' || string[i]=='\r'|| string[i]=='\b')
	  string[i]='\0';
      sprintf(tmp, "%s", string+1); strcat(seq_name, tmp);
      (*nseq)++; continue;
    }
    ptr=string;
    while((ptr!=NULL)&&(*ptr!='\n')&&(*ptr!='\r')&&(*ptr!='\0')&&(*ptr!='\b')){
      if((*ptr=='a')||(*ptr=='A')||(*ptr=='t')||(*ptr=='T')||
	 (*ptr=='g')||(*ptr=='G')||(*ptr=='c')||(*ptr=='C')){
	(*len_dna)++;
      }else if((*ptr!=' ')){
	printf("Wrong character %d in DNA sequence %s: %c\n",
	       *len_dna, inputseq, *ptr);
	printf("%s\n", string); 
	exit(8);
      }
      ptr++;
    }
  }
  if(*nseq==0)*nseq=1;
  *ini_seq=malloc(*nseq*sizeof(int));
  *len_seq=malloc(*nseq*sizeof(int));

  if(seq_name[0]=='\0'){
    strcpy(seq_name, inputseq);
    for(i=0; i<sizeof(seq_name); i++){
      if((seq_name[i]=='_')||(seq_name[i]=='.')){
	seq_name[i]='\0'; break;
      }
    }
  }
  printf("DNA sequence %s\n", seq_name);
  printf("%d sequences of total length %d (%d a.a.)\n",
	 *nseq, *len_dna, *len_dna/3);
  fclose(file_in);

  if(*len_dna==0)return(NULL);

  // Reading
  sequence=(char *)malloc(*len_dna *sizeof(char));
  file_in=fopen(inputseq, "r"); i=0; *nseq=0;
  while(fgets(string, sizeof(string), file_in)!=NULL){
    if(string[0]=='>'){
      (*ini_seq)[*nseq]=i;
      if(*nseq)(*len_seq)[*nseq-1]=i;
      (*nseq)++;
      continue;
    }
    ptr=string;
    while((ptr!=NULL)&&(*ptr!='\n')&&(*ptr!='\r')&&(*ptr!='\0')&&(*ptr!='\b')){
      if((*ptr!=' ')){
	*ptr=Maiuscule(*ptr); sequence[i]=*ptr;
	//int i_nuc=Code_nuc(*ptr);
	i++;
      }
      ptr++;
    }
  }
  fclose(file_in);
  if(*nseq)(*len_seq)[*nseq-1]=i;
  for(i=0; i<*nseq; i++){
    (*len_seq)[i]-=(*ini_seq)[i];
    printf("Seq %d ini= %d len= %d\n", i, (*ini_seq)[i], (*len_seq)[i]);
  }
  
  return(sequence);
}

FILE *open_file(char *name_file, char *ext, short *seq, int lf, char *fit_def)
{
  FILE *file_out; char name[N_CHAR];
  int i, j; float gc=0;
  for(i=0; i<4; i++)if((NUC_CODE[i]=='G')||(NUC_CODE[i]=='C'))gc+=mut_par[i];
  sprintf(name, "%s%s", name_file, ext);
  file_out=fopen(name, "w");
  printf("Writing %s\n", name);
  fprintf(file_out, "# File %s, sequence %s,  PDB %s,  length=%d\n",
	  name_file, seq_name, target.name, len_amm);
  fprintf(file_out, "# fitness: %s\n", fit_def);
  fprintf(file_out, "# %d iterations, random seed: %ld\n", IT_MAX, iran);
  fprintf(file_out, "# Stationary frequencies: ");
  for(i=0; i<4; i++)fprintf(file_out, "%c %.3f ", NUC_CODE[i], mut_par[i]);
  fprintf(file_out, "\n# Transition-transversion ratio= %.2f\n", tt_ratio);
  fprintf(file_out, "# GC bias: %.3f\n", gc);
  fprintf(file_out, "# T= %.3f\n", TEMP);
  fprintf(file_out, "# Config. entropy per residue, unfolded: %.3f", sU1);
  fprintf(file_out, " misfolded: %.3f\n", sC1);
  if(SEC_STR)
    fprintf(file_out, "#Local interactions used, factor= %.3f\n", SEC_STR);
  fprintf(file_out, "# N_pop= %d\n#", N_pop);

  for(i=0; i<len_amm; i++){
    if(i==(i/60)*60)fprintf(file_out,"\n# ");
    fprintf(file_out,"%c", AMIN_CODE[seq[i]]);
  }
  fprintf(file_out,"\n");
  
  if(lf==1){
    fprintf(file_out,
	    "# site aa_old-aa_new  Enat DG fitness syn_subst attempts\n");
    //D(0,n)
  }else if(lf==2){
    fprintf(file_out, "# iter [GC]_3");
    for(i=0; i<3; i++){
      fprintf(file_out, " ");
      for(j=0; j<3; j++)fprintf(file_out, "%c_%d ", NUC_CODE[i], j+1);
    }
    fprintf(file_out, "\n\n");
    /*fprintf(file_out, "  Energy alpha\n");*/
  }
  
  fflush(file_out);
  return(file_out);
}

int Print_dna(char *seq, FILE *file_out, int iter)
{
  short i, j, i_nuc, nuc_count[4][3];
  float length=(float)len_amm, gc3=0;

  for(i=0; i<4; i++)
    for(j=0; j<3; j++)
      nuc_count[i][j]=0;

  j=0;
  for(i=0; i<len_dna; i++){
    i_nuc=Code_nuc(seq[i]);
    nuc_count[i_nuc][j]++; j++;
    if(j==3)j=0;
  }

  /* GC3 */
  for(i=0; i<4; i++)
    if((NUC_CODE[i]=='G')||(NUC_CODE[i]=='C'))gc3+=nuc_count[i][2];
  fprintf(file_out,"%3d  %.3f ", iter, (gc3)/length);
  for(i=0; i<3; i++){
    fprintf(file_out," ");
    for(j=0; j<3; j++){
      fprintf(file_out," %.3f", nuc_count[i][j]/length);
    }
  }
  fprintf(file_out, "\n");
  fflush(file_out);

  return(0);
}

int Get_name(char *name, char *name_seq, int N){
  int i;
  for(i=0; i<N; i++){
    if(strncmp(name_seq+i, ".dna", 4)==0){
       name[i]='\0'; 
       return(0);
    }
    name[i]=name_seq[i];
  }
  printf("ERROR in file name %s, extension .dna not found\n", name);
  exit(8);
}

void Output_name(char *file_name, char *dir_out, char *prot_name,
		 float TEMP, float sU1, float sC1, int MEANFIELD,
		 char *MODEL, float LAMBDA, int OPT_LAMBDA, //NEW
		 int NEUTRAL, int N_pop, float *mut_par)
{
  char name[400];
  if(dir_out[0]!='\0'){sprintf(name, "%s/%s", dir_out, prot_name);}
  else{sprintf(name, "%s", prot_name);}
  if(MEANFIELD){
    //NEW
    if(OPT_LAMBDA){
      sprintf(file_name, "%s_MF_LAMBDA_OPT_%s", name, MODEL);
    }else{
      sprintf(file_name, "%s_MF_LAMBDA%.2f_%s", name, LAMBDA, MODEL);
    }
    //sprintf(file_name, "%s_REM%d_T%.2f_SU%.3f", file_name, REM, TEMP, sU1);
  }else if (NEUTRAL){
    sprintf(file_name, "%s_T%.2f_SU1%.2f_SC1%.2f_NEUTRAL",
	    name, TEMP, sU1, sC1);
  }else{
    sprintf(file_name, "%s_T%.2f_SU1%.2f_SC1%.2f_N%d",
	    name, TEMP, sU1, sC1, N_pop);
  }
  //sprintf(file_name, "%s_GC%.2f", file_name,
  //	  mut_par[Code_nuc('G')]+mut_par[Code_nuc('C')]);
  //if(CpG)sprintf(file_name, "%s_CpG%.0f", file_name, mut_par[4]);
}
 
float Sequence_entropy(double **aa_distr, int L){
  int i, j; float S_sum=0, S, p; double norm=0;

  for(j=0; j<20; j++)norm+=aa_distr[0][j];
  for(i=0; i<L; i++){
    S=0;
    for(j=0; j<20; j++){
      p=aa_distr[i][j]/norm; if(p)S-=p*log(p);
    }
    S_sum+=S;
  }
  return(S_sum/L);
}

void Compute_freq_codons(float *mut_par, float *freq_aa,
			 char **codon, char *coded_aa)
{
  int i, i_aa; float w;
  for(i_aa=0; i_aa<20; i_aa++)freq_aa[i_aa]=0;
  for(i=0; i<64; i++){
    w=mut_par[Code_nuc(codon[i][0])];
    w*=mut_par[Code_nuc(codon[i][1])];
    w*=mut_par[Code_nuc(codon[i][2])];
    if(coded_aa[i]=='*')continue;
    i_aa=Code_AA(coded_aa[i]);
    if(i_aa<0)continue;
    freq_aa[i_aa]+=w;
  }
}

float Sequence_entropy_mut(float *mut_par, char **codon, char *coded_aa)
{
  float freq_aa[20]; int i; float norm=0, S=0, p;

  // Calculating amino acid distribution under mutation alone
  Compute_freq_codons(mut_par, freq_aa, codon, coded_aa);

  // Compute entropy
  for(i=0; i<20; i++)norm+=freq_aa[i];
  for(i=0; i<20; i++){
    if(freq_aa[i]){p=freq_aa[i]/norm; S-=p*log(p);}
  }
  return(S);
}

void Compute_load(double *Tload_sum, double *Tload_dev,
		  double *Mload_sum, double *Mload_dev, int *Nload,
		  struct REM *E_wt, int **C_nat, int *i_sec, short *aa_seq,
		  float fitness_wt, char *dna_seq, int len_dna, 
		  char **codon, char *coded_aa)
{
  double translation_load=0, mutation_load=0;

  // Folding thermodynamics
  float fitness, DeltaG;
  struct REM E_mut;
  Initialize_E_REM(&E_mut, E_wt->L,E_wt->REM,
		   E_wt->T,E_wt->S_C,E_wt->S_U, FILE_STR, 0);

  /*
    Load=Sum_j P(nat->Seq_j)[finess(nat)-fitness(Seq_j)]
    P(nat->Seq_j) is non zero only if sequence j is one base mutation from
    the native sequence.
    Translation load: P(nat->Seq_j)=1
    Mutation load: P(nat->Seq_j)=Mutation probability,
    i.e. f(new base) times 1 if transversion, times tt_ratio if transition    
   */

  // Mutations
  int pos=-1, res_mut=0, aa_old=-1;
  char *codon_nat=dna_seq, codon_mut[3];
  int sum_trans=0, sum_mut=0;

  for(int nuc_mut=0; nuc_mut<len_dna; nuc_mut++){
    
    pos++;
    if(pos==3){
      pos=0; res_mut++; codon_nat=dna_seq+res_mut*3;
    }
    for(int j=0; j<3; j++)codon_mut[j]=codon_nat[j];
    int nuc_wt=Code_nuc(dna_seq[nuc_mut]);

    for(int base=0; base<4; base++){

      // Mutate amino acid
      if(base==nuc_wt)continue;
      codon_mut[pos]=NUC_CODE[base];
      int aa_new=Coded_aa(codon_mut, codon, coded_aa);
      if(aa_new==aa_old)continue;         // Synonymous
      if(aa_new<0){fitness=0; goto load;} // Stop codon

      // Folding thermodynamics
      Copy_E_REM(&E_mut, E_wt);
      DeltaG=
	Mutate_DG_overT_contfreq(&E_mut, aa_seq, C_nat,
				 i_sec, res_mut, aa_new);
      fitness=1./(1+exp(DeltaG));
      
    load:
      // Translation load
      sum_trans++;
      translation_load+=fitness;

      // Mutation load
      float p=mut_par[base];
      if(base==Transition(NUC_CODE[nuc_wt]))p*=tt_ratio;
      sum_mut+=p;
      mutation_load+=fitness*p;
    }
  }
  Empty_E_REM(&E_mut);

  translation_load = fitness_wt-(translation_load)/sum_trans;
  mutation_load = fitness_wt-(mutation_load)/sum_mut;

  *Tload_sum += translation_load;
  *Tload_dev += translation_load*translation_load;
  *Mload_sum += mutation_load;
  *Mload_dev += mutation_load*mutation_load;
  (*Nload)++;
}

unsigned long randomgenerator(void){
     
     unsigned long tm;
     time_t seconds;
     
     time(&seconds);
     srand((unsigned)(seconds % 65536));
     do   /* waiting time equal 1 second */
       tm= clock();
     while (tm/CLOCKS_PER_SEC < (unsigned)(1));
     
     return((unsigned long) (rand()));

}

int Selection(float fitness, float fitness_old, int N_pop)
{
  // Moran's process:
  /* P_fix = (1-exp(-Df))/(1-exp(-N*Df)) */
  if(fitness<=0)return(0);
  //if((int)N_pop==1)return(1);
  double f_ratio= fitness_old / fitness;
  double x= (1.-f_ratio)/(1.-pow(f_ratio, N_pop));
  if(VBT){
    float rand=RandomFloating();
    printf("es= %.2f x= %.2g rand= %.2g\n", f_ratio, x, rand);
    if(rand < x)return(1);
  }else{
    if(RandomFloating() < x)return(1);
  }
  return(0);
}

int Detailed_balance(float *p, int xend, int xini)
{
  if(p[xend]>=p[xini])return(1);
  if(RandomFloating() < p[xend]/p[xini])return(1);
  return(0);
}

float *Get_counts(short *seq, int L, int Naa)
{
  float *num=malloc(Naa*sizeof(float));
  int i; for(i=0; i<Naa; i++)num[i]=0;
  for(i=0; i<L; i++){
    if((seq[i]<0)||(seq[i]>=Naa)){
      printf("ERROR, wrong symbol %d at site %d\n", seq[i], i);
      exit(8);
    }
    num[seq[i]]++;
  }
  //for(i=0; i<Naa; i++)num[i]/=L;
  return(num);
 }

void Record_evo(float *fit_evo, double fitness,
		float *DG_evo,  double DG,
		float *Enat_evo, double E_nat, int *nsam)
{
  fit_evo[*nsam]=fitness;
  DG_evo[*nsam]=DG;
  Enat_evo[*nsam]=E_nat;
  (*nsam)++;
}


int WildType_DDG(float **DG_mut, int **C_nat, int *i_sec,
		 short *aa_seq, struct REM *E_wt)
{
  int L=E_wt->L; float DG;
  printf("Wild type mutants. DG_wt=%.2f REM=%d\n",
	 E_wt->DeltaG, E_wt->REM);

  struct REM E_mut;
  Initialize_E_REM(&E_mut, L, E_wt->REM,
		   E_wt->T, E_wt->S_C, E_wt->S_U, FILE_STR, 0);

  for(int res_mut=0; res_mut<L; res_mut++){
    float *DDG=DG_mut[res_mut];
    //printf("%d  ", res_mut);
    for(int aa=0; aa<20; aa++){
      if(aa==aa_seq[res_mut]){
	DDG[aa]=0;
      }else{
	Copy_E_REM(&E_mut, E_wt);
	DG=Mutate_DG_overT_contfreq(&E_mut, aa_seq, C_nat, i_sec, res_mut, aa);
	DDG[aa]=DG-E_wt->DeltaG;
      }
    }
  }
  printf("%d mutants computed\n", L*19);
  Empty_E_REM(&E_mut);
  return(0);
}


int Optimize_distr(struct MF_results *opt_res, float **P_WT_ia,
		   float **exp1, float **exp2, float *P_mut_a,
		   float **f_reg_ia, float **f_msa_ia, float *wi,
		   struct REM *E_wt, int **C_nat, int *i_sec,
		   char *name_file,FILE *file_summary,char *label,int repeat)
{
  int L=E_wt->L; float Lambda[2]; Lambda[1]=0;
  opt_res->score=-10000;

  if(OPT_REG==0){
    printf("#Selection model: %s\n", label);
    printf("#Optimizing Lambda by ");
    if(LAMBDA_ANALYTIC){printf("derivative=0\n");}
    else{printf("maximizing score\n");}
    printf("#Lambda score\n");
  }

  if(LAMBDA_ANALYTIC){
    Analytic_Lambda(Lambda, P_WT_ia, exp1, exp2, P_mut_a, L, wi, f_reg_ia);
  }else{
    Maximize_Lambda(Lambda, P_WT_ia, exp1, exp2, P_mut_a, L, wi, f_reg_ia,
		    f_msa_ia, repeat);
  }

  if(isnan(Lambda[0])|| (exp2 && isnan(Lambda[1]))){
    printf("ERROR in Optimize_distr, Lambda is nan\n");
  }
  int zero=Compute_P_WT(P_WT_ia, Lambda, exp1, exp2, P_mut_a, L, PMIN);
  for(int k=0; k<2; k++)opt_res->Lambda[k]=Lambda[k];
  //Compute_score(opt_res, P_WT_ia, L, f_reg_ia, f_msa_ia, wi);
  Test_distr(opt_res, P_WT_ia, f_reg_ia, f_msa_ia, wi, L,C_nat,i_sec,*E_wt);
  double KLD=0; for(int i=0; i<L; i++)KLD+=KL(P_WT_ia[i], P_mut_a, 20);
  opt_res->KL_mut=KLD/L;
  Print_results(*opt_res, label, file_summary);
  if(OPT_REG==0)
    printf("Model %s Optimal score= %.3f Lambda=%.3f %.3f Entropy= %.3f\n",
	   label, opt_res->score, opt_res->Lambda[0], opt_res->Lambda[1],
	   opt_res->entropy);
  return(zero);
}

int Maximize_Lambda(float *Lambda, float **P_WT_ia,
		    float **exp1, float **exp2, float *P_mut_a, int L,
		    float *wi, float **f_reg_ia, float **f_msa_ia, int repeat)
{
  int REDO=0;
  float PMIN2; if(PMIN_ZERO){PMIN2=0;}else{PMIN2=PMIN;}
  float STEP=0.8, step_rate=0.75, step, eps=0.0001;
  // Step of lambda and its reduction at each iteration
  int IT_MAX=200, iter, k, end;

  int redo; if(REDO && repeat){redo=1;}else{redo=0;}

  float x[4], y[4], Lam[2];
  int n, nmod; if(exp2){nmod=2;}else{nmod=1;}
  for(n=0; n<2; n++)Lam[n]=Lambda_start[n]; 
  if(nmod==2)for(n=0; n<2; n++)if(Lam[n]<1)Lam[n]=1;
  struct MF_results res, opt_res, tmp_res; opt_res.score=-100000;
  
 restart:
  end=0; step=STEP;
  // Initialize
  Compute_P_WT(P_WT_ia, Lam, exp1, exp2, P_mut_a, L, PMIN2);
  Compute_score(&tmp_res, P_WT_ia, L, f_reg_ia, f_msa_ia, wi);
  for(n=0; n<2; n++)tmp_res.Lambda[n]=Lam[n];
  if(OPT_REG==0)
    printf("%.3g %.3g %.3g n=%d\n",tmp_res.score,Lam[0],Lam[1],nmod);

  int n_down=0;
  for(iter=0; iter<IT_MAX; iter++){
    if(n_down>=3)break;
    for(n=0; n<nmod; n++){
      x[1]=tmp_res.Lambda[n];
      y[1]=tmp_res.score;
      for(k=0; k<4; k++){
	if(k==1){
	  continue;
	}else if(k==0){
	  Lam[n]=x[1]*(1-step);
	}else if(k==2){
	  Lam[n]=x[1]*(1+step);
	}else{ // k==3
	  if(y[1] > y[0] && y[1] > y[2]){end++; if(end==nmod)break;}
	  else{end=0;}
	  Lam[n]=Find_max_quad(x[0],x[1],x[2],y[0],y[1],y[2],0,200);
	  if(isnan(Lam[n])){printf("ERROR in find_max\n");}
	}
	Compute_P_WT(P_WT_ia, Lam, exp1, exp2, P_mut_a, L, PMIN2);
	Compute_score(&res, P_WT_ia, L, f_reg_ia, f_msa_ia, wi);
	//if(res.entropy<=Entropy_min)break;
	if(isnan(res.score))break;
	x[k]=Lam[n];
	y[k]=res.score;
	if(k==3){ // k=3
	  if(OPT_REG==0)
	    printf("%.4g %.4g %.4g %d\n",res.score,Lam[0],Lam[1],n_down);
	  if(fabs(res.score-tmp_res.score)<eps){end++;}else{end=0;}
	  if(res.score > tmp_res.score){n_down=0;}
	  else{n_down++;}
	}
	if(res.score > tmp_res.score){
	  tmp_res=res; for(int i=0; i<nmod; i++)tmp_res.Lambda[i]=Lam[i]; 
	}
      } // end k (4 samples for quadratic optimization)
      if(end==nmod)break;
      Lam[n]=tmp_res.Lambda[n];
    } // end n (model components)
    if(step>0.01)step*=step_rate;
  } // end iter
  if(iter==IT_MAX)
    printf("WARNING, optimization of Lambda did not converge\n");

  float L0[2], L1[2], y0[2], y1[2], *yy, *LL;
  for(n=0; n<nmod; n++)Lam[n]=tmp_res.Lambda[n];
  for(n=0; n<nmod; n++){
    if(n==0){LL=L0; yy=y0;}else{LL=L1; yy=y1;}
    Lam[n]=tmp_res.Lambda[n]; LL[1]=Lam[n]; yy[1]=tmp_res.score;
    for(k=0; k<=2; k+=2){
      if(k==0){LL[0]=LL[1]*(1-step); Lam[n]=LL[0];}
      else{LL[2]=LL[1]*(1+step); Lam[n]=LL[2];}
      Compute_P_WT(P_WT_ia, Lam, exp1, exp2, P_mut_a, L, PMIN2);
      Compute_score(&res, P_WT_ia, L, f_reg_ia, f_msa_ia, wi);
      yy[k]=res.score;
    }
    if(yy[1] < yy[0] || yy[1] < yy[2]){
      printf("WARNING, optimization of Lambda by bisection not possible\n");
      printf("n=%d x: %.6g %.6g %.6g y: %.6g %.6g %.6g it= %d d= %d end= %d\n",
	     n, LL[0],LL[1],LL[2],yy[0],yy[1],yy[2],iter,n_down,end);
      Lam[n]=LL[1];
      goto last_lambda;
    }
  }
  
  // Look for optimum dividing intermediate interval
  end=0; int wrong=0;
  for(iter=0; iter<30; iter++){
    for(n=0; n<nmod; n++){
      if(n==0){LL=L0; yy=y0;}else{LL=L1; yy=y1;}
      if(yy[1]<yy[0] || yy[1]<yy[2] || LL[0]>LL[1] || LL[1]>LL[2]){
	wrong=1; break;
      }
      float xa=0.5*(LL[0]+LL[1]); Lam[n]=xa;
      Compute_P_WT(P_WT_ia, Lam, exp1, exp2, P_mut_a, L, PMIN2);
      Compute_score(&res, P_WT_ia, L, f_reg_ia, f_msa_ia, wi);
      if(res.score > tmp_res.score){tmp_res=res; tmp_res.Lambda[n]=xa;}
      float ya=res.score;
      float xb=0.5*(LL[1]+LL[2]); Lam[n]=xb;
      Compute_P_WT(P_WT_ia, Lam, exp1, exp2, P_mut_a, L, PMIN2);
      Compute_score(&res, P_WT_ia, L, f_reg_ia, f_msa_ia, wi);
      if(res.score > tmp_res.score){tmp_res=res; tmp_res.Lambda[n]=xb;}
      float yb=res.score;
      if(ya > yb && ya > yy[1]){
	LL[2]=LL[1]; yy[2]=yy[1]; LL[1]=xa; yy[1]=ya; // max at xa
      }else if(yb > ya && yb > yy[1]){
	LL[0]=LL[1]; yy[0]=yy[1]; LL[1]=xb; yy[1]=yb; // max at xb
      }else{
	LL[0]=xa; yy[0]=ya; LL[2]=xb; yy[2]=yb; // max at x1
      }
      Lam[n]=LL[1];
      if(fabs(yy[1]-yy[2])< eps || fabs(yy[1]-yy[0])< eps){
	end++; if(end==nmod)break;
      }else{
	end=0;
      }
    }
    if(wrong)break;
  }

 last_lambda:
  if(tmp_res.score > opt_res.score){
    opt_res=tmp_res;
    for(n=0; n<nmod; n++){
      opt_res.Lambda[n]=tmp_res.Lambda[n];
      Lambda[n]=opt_res.Lambda[n];
      if(isnan(Lambda[n]))
	printf("ERROR in numeric, Lambda[%d]= %.3g\n",n,Lambda[n]);
    }
  }
  if(redo){
    redo=0; for(n=0; n<nmod; n++)Lam[n]=tmp_res.Lambda[n]; goto restart;
  }
  return(0);
}

int Analytic_Lambda(float *Lambda, float **P_WT_ia,
		    float **exp1, float **exp2, float *P_mut_a, int L,
		    float *wi, float **f_reg_ia)
{
  int IT_MAX=200, iter; float eps=0.000001;
  float Lambda_opt[2], KL_opt;

  int n, nmod; if(exp2){nmod=2;}else{nmod=1;}
  double wQFreg[2], **lFreg[2]; int i,a;
  for(n=0; n<nmod; n++){
    wQFreg[n]=0; lFreg[n]=Allocate_mat2_d(L, 20, "lFreg");
  }
  for(i=0; i<L; i++){
    for(n=0; n<nmod; n++){
      double QF=0, *lF=lFreg[n][i]; float *freg_i=f_reg_ia[i];
      float *qi; if(n==0){qi=exp1[i];}else{qi=exp2[i];}
      for(a=0; a<20; a++){
	QF+=freg_i[a]*qi[a];
	lF[a]=log(freg_i[a]/P_mut_a[a]);
      }
      wQFreg[n]+=wi[i]*QF;
    }
  }

  float Lambda_new[2]; Lambda[1]=0; Lambda_new[1]=0;
  for(n=0; n<nmod; n++){
    Lambda[n]=Lambda_start[n]; Lambda_new[n]=Lambda[n];
  }
  double KL_old=-1;
  for(iter=0; iter<IT_MAX; iter++){
    double wQ1[2], wQ2[2], wPF[2], wQQ=0, wQ1Q2=0, KL=0;
    for(n=0; n<nmod; n++){wQ1[n]=0; wQ2[n]=0; wPF[n]=0;}
    Compute_P_WT(P_WT_ia, Lambda, exp1, exp2, P_mut_a, L, 0);
    for(i=0; i<L; i++){
      float *P_i=P_WT_ia[i]; double q11=0;
      for(n=0; n<nmod; n++){
	double q1=0, q2=0, PF=0, PFq=0, q1q2=0;
	float *qi, *qqi=NULL; double *lF=lFreg[n][i];
	if(n==0){qi=exp1[i];}else{qi=exp2[i]; qqi=exp1[i];}
	for(a=0; a<20; a++){
	  double Pq=P_i[a]*qi[a]; q1+=Pq; q2+=Pq*qi[a];
	  double Pl=P_i[a]*lF[a]; PF+=Pl; PFq+=Pl*qi[a];
	  if(n)q1q2+=Pq*qqi[a];
	}
	wQ1[n]+=wi[i]*q1;
	wQ2[n]+=wi[i]*(q2-q1*q1);
	wPF[n]+=wi[i]*(q1*PF-PFq);
	if(n){wQQ+=wi[i]*q1q2; wQ1Q2+=wi[i]*q1*q11;}
	else{q11=q1;}
      }
      double KLi=KL_symm(P_i, f_reg_ia[i], 20); KL+=wi[i]*KLi;
    }
    double det, wQ12, B[2];
    if(nmod==1){
      det=wQ2[0];
    }else{
      wQ12=wQQ-wQ1Q2;
      det=wQ2[0]*wQ2[1]-wQ12*wQ12;
    }
    for(n=0; n<nmod; n++){
      B[n]=(wPF[n]+wQ1[n]-wQFreg[n]);
      if(wQ2[n]<=0){
	printf("ERROR in Lambda computation, n=%d Lambda=%.3g variance %.3g\n",
	       n, Lambda[n], wQ2[n]); break;
      }
    }
    for(n=0; n<nmod; n++){
      if(nmod==1){Lambda_new[0]=B[0];}
      else if(n==0){Lambda_new[0]=B[0]*wQ2[1]-B[1]*wQ12;}
      else if(n==1){Lambda_new[1]=B[1]*wQ2[0]-B[0]*wQ12;}
      Lambda_new[n]/=det;
      if(Lambda_new[n]<0){
	printf("ERROR, n=%d det=%.3g Lambda_new= %.3g, Lambda= %.3g",
	       n, det, Lambda_new[n], Lambda[n]);
	Lambda_new[n]=Lambda[n]/2;
	printf(" -> %.3g\n", Lambda_new[n]);
	// break; //exit(8);
      }
    }

    if(OPT_REG==0){
      printf("%.6g %.6g", KL, Lambda[0]);
      if(exp2)printf(" %.6g", Lambda[1]);
      printf("\n");
    }
    if(iter==0 || KL<KL_opt){
      KL_opt=KL; for(n=0; n<nmod; n++)Lambda_opt[n]=Lambda[n];
    }
    if(fabs(KL-KL_old)<eps*KL_old)break;
    if(KL >= KL_old){ // KL increases
      for(n=0; n<nmod; n++)Lambda_new[n]=0.3*Lambda_new[n]+0.7*Lambda[n];
    }
    KL_old=KL;
    for(n=0; n<nmod; n++)Lambda[n]=Lambda_new[n];
  }
  printf("Optimizing Lambda, KL=%.4g L1=%.4g", KL_opt, Lambda_opt[0]);
  if(exp2)printf(" L2=%.4g", Lambda_opt[1]);
  printf("\n");
  if(iter==IT_MAX){
    printf("WARNING, Lambda could not be optimized in %d iterations\n",iter);
  }
  for(n=0; n<nmod; n++)Empty_matrix_d(lFreg[n], L);
  for(n=0; n<nmod; n++)Lambda[n]=Lambda_opt[n];
  return(0);
}

int Compute_P_WT(float **P_WT_ia, float *Lambda,
		 float **exp1, float **exp2, float *P_mut_a,
		 int L, float Pmin)
{
  int i, a, zero=0, error=0;
  if(isnan(Lambda[0]) || (exp2 && isnan(Lambda[1]))){
    printf("WARNING in Compute_P, Lambda is nan: %.3g %.3g\n",
	   Lambda[0],Lambda[1]);
    for(i=0; i<2; i++)if(isnan(Lambda[i]))Lambda[i]=Lambda_start[i];
    printf("Setting Lambda to  %.3g %.3g\n", Lambda[0],Lambda[1]);
  } 
  for(i=0; i<L; i++){
    float *P_WT=P_WT_ia[i], *e1=exp1[i]; double Z=0;
    for(a=0; a<20; a++){
      double ee=Lambda[0]*e1[a];
      if(exp2)ee+=Lambda[1]*exp2[i][a];
      P_WT[a]=P_mut_a[a]*exp(-ee);
      if(P_WT[a]<Pmin){P_WT[a]=Pmin; zero++;}
      Z+=P_WT[a];
    }
    if(isnan(Z) || Z<=0){
      error++; int n, nmod=1; if(exp2)nmod=2;
      printf("ERROR, i=%d Z= %.3g\n", i, Z);
      for(n=0; n<nmod; n++){
	printf("Lam= %.3g Q= ",Lambda[n]); if(n)e1=exp2[i];
	for(a=0; a<20; a++)printf(" %.2g", e1[a]);
	printf("\n");
      }
    }
    for(a=0; a<20; a++)P_WT[a]/=Z;
  }
  if(error)exit(8);
  return(zero);
}

float KL_symm(float *P, float *Q, int n){
  double KL=0;
  for(int i=0; i<n; i++){
    if(P[i]<=0 || Q[i]<=0)continue;
    KL+=(P[i]-Q[i])*log(P[i]/Q[i]);
  }
  return(KL);
}

void Print_matrix(struct protein target){
  char name_out[200];
  sprintf(name_out, "%s_Contact_matrix.cm", target.name);
  FILE *file_out=fopen(name_out, "w"); int i;
  printf("Writing contact matrix in file %s\n", name_out);
  fprintf(file_out, "# %d %d %s\n",
	  target.length, target.n_cont, target.name);
  for(i=0; i<target.length; i++){
    short *Ci=target.contact[i];
    while(*Ci>=0){
      fprintf(file_out, "%d %d\n", i, *Ci); Ci++;
    }
  }
  fclose(file_out);
}

void Initialize_load(struct load *load){
  load->df_ave=0; load->df_dev=0;
  load->dG_ave=0; load->dG_dev=0;
  load->num=0;
}

void Sum_loads(struct load *load, struct load *load1){
  load->df_ave+=load1->df_ave;
  load->df_dev+=(load1->df_ave)*(load1->df_ave);
  load->dG_ave+=load1->dG_ave;
  load->dG_dev+=(load1->dG_ave)*(load1->dG_ave);
  load->num++;
}

FILE *Open_summary(char *name_file, struct REM E_wt,
		   float *mut_par, short *aa_seq, float Seq_id)
{
  // Print summary of results
  char name[100]; int L=E_wt.L;
  sprintf(name, "%s%s", name_file, EXT_SUM);
  printf("Writing %s\n", name);
  FILE *file_out=fopen(name, "w");
  fprintf(file_out, "# Loc.interactions: %.3f ", SEC_STR);
  fprintf(file_out, "T= %.2f SC=%.3f SU= %.3f REM= %d T_freezing= %.2f\n",
	  E_wt.T, E_wt.S_C/L, E_wt.S_U/L, E_wt.REM, E_wt.Tf);
  name[5]='\0';
  // Wild type sequence
  double h_wt=0; for(int i=0; i<L; i++)h_wt+=hydro[aa_seq[i]]; h_wt/=L;
  fprintf(file_out,
	  "# PDB= \"%s\" L_seq=%3d DG_wt= %.1f /L= %.3f Tf= %.2f h= %.3f\n",
	  name, L, E_wt.DeltaG, E_wt.DeltaG/L, E_wt.Tf, h_wt);
  fprintf(file_out,
	  "# %d residues in PDB sequence, %d structured %d disordered\n"
	  "# %d columns in MSA, %d residues do not have associated col.\n",
	  L, L_PDB, L-L_PDB, L_ali, L_noali);
  /*fprintf(file_out, "#L=%3d   nuc_freq= %.3f %.3f %.3f %.3f", L,
	  mut_par[Code_nuc('G')], mut_par[Code_nuc('C')],
	  mut_par[Code_nuc('A')], mut_par[Code_nuc('T')]);
  fprintf(file_out, "   tt= %.1f CpG= %.1f twonuc= %.1g\n",
  mut_par[4], mut_par[5], mut_par[6]);*/
  fprintf(file_out,
	  "# Lambda is obtained minimizing KL(mod,reg.obs)+KL(reg.obs,mod)\n");
  fprintf(file_out, "# KL(reg.obs,mod): -likelihood(reg|model)-Entropy(reg)\n");
  fprintf(file_out, "# KL(mod,reg.obs): -likelihood(model|reg)-Entropy(mod)\n");
  fprintf(file_out,
	  "# MSA profiles regularized as f_ia=(n(MSA_ia)+REG*f_a)/N");
  fprintf(file_out, "  REG=%.3f\n",REG_FACT);
  fprintf(file_out, "# Mean sequence identity between PDB seq and MSA: %.3f\n",
	  Seq_id);
  return(file_out);
}

void Print_TN_div(short *aa_seq, short *aa_seq0, int L,
		  int num_aa_subst, FILE *file_out)
{
  float SI0=0.06;
  float SI=0;
  for(int i=0; i<L; i++)if(aa_seq[i]==aa_seq0[i])SI++;
  SI/=L; if(SI<=SI0)return;
  float TN=-log((SI-SI0)/(1-SI0));
  fprintf(file_out, "%.3f\t%d\n", TN, num_aa_subst);
}

void Print_seq(FILE *file_msa, short *aa_seq, int len_amm, int *nseq_msa,
	       float DeltaG)
{
  if(file_msa==NULL)return;

  fprintf(file_msa,">Seq%d_DG=%.4g", *nseq_msa, DeltaG);
  int j=0;
  for(int i=0; i<len_amm; i++){
    if(j==0)fprintf(file_msa,"\n");
    j++; if(j==60)j=0;
    fprintf(file_msa,"%c",AMIN_CODE[aa_seq[i]]);
  }
  fprintf(file_msa,"\n");
  (*nseq_msa)++;
}

void Regularize(float **f_reg_ia, float **f_msa_ia, float *f_aa, int len_amm,
		float w_max, float reg)
{
  for(int i=0; i<len_amm; i++){
    double sum=0; //float reg=w_max-wi[i]+w_max*REG_FACT;
    for(int a=0; a<20; a++){
      f_reg_ia[i][a]=f_msa_ia[i][a]/w_max+reg*f_aa[a];
      //f_reg_ia[i][a]=f_msa_ia[i][a]/wi[i]+reg*f_aa[a];
      sum+=f_reg_ia[i][a];
    }
    for(int a=0; a<20; a++){f_reg_ia[i][a]/=sum;}
  }
}

void Compute_nuc_mut(char *dna_seq, int len_dna,
		     short *aa_seq, int len_amm,
		     char **codon, char *coded_aa,
		     char *SSC_TYPE, float **exp1, float **exp2,
		     float *Lambda, FILE *file_out)
{
  int n, nmod=1; if(exp2)nmod=2;

  int a, b, i, j;
  double DDG_sum[4][4]; int m_sum[4][4];
  for(a=0; a<4; a++){
    for(b=0; b<4; b++){DDG_sum[a][b]=0; m_sum[a][b]=0;}
  }

  int in=0; char New_cod[3];
  for(i=0; i<len_amm; i++){
    float DDG_max[2]; DDG_max[1]=0;
    for(n=0; n<nmod; n++){
      float *ee; if(n==0){ee=exp1[i];}else{ee=exp2[i];}
      DDG_max[n]=ee[0];
      for(j=1; j<20; j++)if(ee[j]>DDG_max[n])DDG_max[n]=ee[j];
    }
    for(j=0; j<3; j++)New_cod[j]=dna_seq[in+j];
    for(j=0; j<3; j++){
      char old_nuc=dna_seq[in];
      a=Code_nuc(old_nuc);
      for(b=0; b<4; b++){
	if(b==a)continue;
	New_cod[j]=NUC_CODE[b];
	int aa_new=Coded_aa(New_cod, codon, coded_aa);
	if(aa_new<0){
	  DDG_sum[a][b]+=Lambda[0]*(len_amm-i)*DDG_max[0];
	  if(exp2)DDG_sum[a][b]+=Lambda[1]*(len_amm-i)*DDG_max[1];
	}else{
	  DDG_sum[a][b]+=Lambda[0]*exp1[i][aa_new];
	  if(exp2)DDG_sum[a][b]+=Lambda[1]*exp2[i][aa_new];
	}
	m_sum[a][b]++;
      }
      New_cod[j]=old_nuc; in++;
    }
  }

  fprintf(file_out, "# Effect of mutations of type %s\n", SSC_TYPE);
  fprintf(file_out,"#a\tb\tDDG(a,b)/Ldna\tnmut\tDDG(a,b)/nmut\n");
  fprintf(file_out, "#DDG represents the total estimated fitness cost of ");
  fprintf(file_out, " mutations a->b, whose number is nmut\n");
  fprintf(file_out, "# Ldna= %d Lambda= %.3g", len_dna, Lambda[0]);
  if(exp2)fprintf(file_out, " %.3g", Lambda[1]);
  fprintf(file_out, "\n");

  for(a=0; a<4; a++){
    for(b=0; b<4; b++){
      if(b==a)continue;
      float df=DDG_sum[a][b]/len_dna;
      fprintf(file_out, "%c\t%c", NUC_CODE[a], NUC_CODE[b]);
      fprintf(file_out, "\t%.5f", df);
      fprintf(file_out, "\t%d", m_sum[a][b]);
      fprintf(file_out, "\t%.5f\n", DDG_sum[a][b]/m_sum[a][b]);
    }
  }

}

int Match_dna(char *dna_seq, int nseq, int *ini_seq, int *len_seq,
	      struct protein pdb, char **codon, char *coded_aa)
{
  int nchain=pdb.nchain, ic, i;
  if(nchain!=nseq){
    printf("ERROR, %d different chains but %d dna sequences,", nchain, nseq);
    printf(" discarding the DNA sequences\n"); return(0);
  }
  int match[nchain]; for(int i=0; i<nchain; i++)match[i]=-1;
  char *aa_trans[nseq]; int len_trans[nseq];
  for(i=0; i<nseq; i++){
    len_trans[i]=Translate_aa(&(aa_trans[i]),dna_seq+ini_seq[i],len_seq[i],
			      codon, coded_aa);
    printf("Translated sequence %d: ",i);
    for(int j=0; j<len_trans[i]; j++)printf("%c", aa_trans[i][j]);
    printf("\n");
  }

  for(ic=0; ic<nchain; ic++){
    short *aa_seq=pdb.aa_seq+pdb.ini_chain[ic];
    int len_amm=pdb.len_chain[ic];
    char aa_char[len_amm];
    for(i=0; i<len_amm; i++)aa_char[i]=AMIN_CODE[aa_seq[i]];
    for(i=0; i<nseq; i++){
      if(Compare_amm(aa_char, len_amm, aa_trans[i], len_trans[i])){
	match[ic]=i; break;
      }
    }
    if(match[ic]<0){
      printf("ERROR, no match found for PDB seq %d %d a.a.\n", ic, len_amm);
      printf("Candidate matches (a.a.): ");
      for(i=0; i<nseq; i++)printf(" %d", len_seq[i]/3);
      printf("\n");
      return(0);
    }
  }
  int equal=1;
  for(ic=0; ic<nchain; ic++)if(match[ic]!=ic){equal=0; break;}
  if(equal)return(1); // Order must not be changed

  // Change order of sequences
  int len_dna=0; for(i=0; i<nseq; i++)len_dna+=len_seq[i];
  char seq_tmp[len_dna]; int len_tmp[nseq];
  for(i=0; i<len_dna; i++)seq_tmp[i]=dna_seq[i];
  for(i=0; i<nseq; i++){len_tmp[i]=len_seq[i];}
  int ini=0; 
  for(ic=0; ic<nchain; ic++){
    int idna=match[ic], j=ini;
    ini_seq[ic]=ini; len_seq[ic]=len_tmp[idna];
    for(i=0; i<len_seq[ic]; i++){dna_seq[j]=seq_tmp[i]; j++;}
    ini+=len_seq[ic];
  }
  return(1);
}

double Optimize_reg(struct MF_results *opt_res, float **P_opt_ia,
		    float reg_ini, float w_max, float *f_aa,
		    struct MF_results *res, float **P_ia,
		    float **exp1,float **exp2, float *P_mut_a,
		    float **f_reg_ia, float **f_msa_ia,
		    float *wi, struct REM *E_wt, int **C_nat, int *i_sec,
		    char *name_file, FILE *file_summ, char *label)
{
  float step=0.02, reg_max=0.85; int itmax=1;
  int N_reg=reg_max/step, k;
  float reg=0, reg_step=step/REG_COEF;
  float Lambda[N_reg][2], L_opt[2], reg_opt=-1, score_opt=-10000;
  float reg_k[N_reg], score_k[N_reg], lik_k[N_reg], symm_k[N_reg], KL[N_reg];

  char name_out[200]; sprintf(name_out, "%s_Cv.dat", name_file);
  FILE *file_out=fopen(name_out, "w");
  printf("Writing %s\n", name_out);

  int nmod=1, n; if(exp2){nmod=2;}else{L_opt[1]=0; res->Lambda[1]=0;}
  for(n=0; n<nmod; n++)res->Lambda[n]=opt_res->Lambda[n];

  char out[200], tmp[80];
  strcpy(out, "# Optimizing the parameter reg by ");
  if(SCORE_CV){
    strcat(out," maximizing d(lik_MSA)/d(reg)\n");
  }else{
    strcat(out, "minimizing |KL_mod-KL_reg|\n");
  }
  fprintf(file_summ, "%s", out);
  fprintf(file_out, "%s", out);
  fprintf(file_out, "#1=reg 2=Cv 3=lik 4=KL 5=|KL_mod-KL_reg| 6=Lambda");
  if(nmod==2)fprintf(file_out, " 7=Lambda2");
  fprintf(file_out, "\n");

  for(k=0; k<N_reg; k++){
    reg+=reg_step; reg_k[k]=reg;
    Compute_score_reg(res, reg, w_max, f_aa, P_ia, exp1, exp2, P_mut_a,
		      f_reg_ia,f_msa_ia,wi, E_wt, C_nat, i_sec,
		      name_file, file_summ, label);
    lik_k[k]=res->lik_MSA;
    symm_k[k]=fabs(res->KL_mod-res->KL_reg);
    KL[k]=res->KL_mod+res->KL_reg;
    for(n=0; n<nmod; n++)Lambda[k][n]=res->Lambda[n];
  }
  for(k=0; k<(N_reg-1); k++){
    float Cv;
    if(k){
      Cv=(lik_k[k-1]-lik_k[k+1])/(2*reg_step);
    }else{
      Cv=(lik_k[k]-lik_k[k+1])/reg_step;
    }
    if(SCORE_CV){score_k[k]=Cv;}else{score_k[k]=-symm_k[k];}
    fprintf(file_out, "%.4g\t%.4g\t%.4g\t%.4g\t%.4g\t%.4g",
	    reg_k[k]*REG_COEF,Cv,lik_k[k],KL[k],symm_k[k],Lambda[k][0]);
    if(exp2)fprintf(file_out, "\t%.4g",Lambda[k][1]);
    fprintf(file_out, "\n");
  }

  // Find maximum by bisection
  //Lambda_start=Lambda[0]; res->Lambda=Lambda_start;
  for(k=0; k<(N_reg-1); k++){	    

    float Lambda_thr[2];
    for(n=0; n<nmod; n++)
      if(k){Lambda_thr[n]=0.5*Lambda[k-1][n];}else{Lambda_thr[n]=0;}
    if(k && (Lambda[k][0]<=Lambda_thr[0]||Lambda[k][1]<=Lambda_thr[1])
       && (lik_k[k-1]>lik_k[k+1])){ // Cv>0
      // phase transition! Small Lambda, mutational distribution
      fprintf(file_out,"# Sudden drop in Lambda at reg= %.2g",
	      reg_k[k]*REG_COEF);
      for(n=0; n<nmod; n++)
	fprintf(file_out," %.4g -> %.4g",Lambda[k-1][n], Lambda[k][n]);
      fprintf(file_out," exiting\n"); break; 
    }

    if(score_k[k]>score_opt){
      score_opt=score_k[k]; reg_opt=reg_k[k];
      for(n=0; n<nmod; n++)L_opt[n]=Lambda[k][n];
    }

    if(k && (score_k[k]>0 || SCORE_CV==0) &&
       score_k[k]>score_k[k-1] && score_k[k]>score_k[k+1]){
      int i=k;float x2=reg_k[i], y2=score_k[i];
      i=k-1;  float x1=reg_k[i];
      i=k+1;  float x3=reg_k[i];
      for(int iter=0; iter<itmax; iter++){
	float xa=0.5*(x1+x2), ya, Cv, symm;
	Cv=Compute_Cv(res, xa, reg_step, w_max,f_aa,
		      P_ia,exp1,exp2,P_mut_a,f_reg_ia,f_msa_ia,wi,
		      E_wt,C_nat,i_sec, name_file, file_summ, label);
	symm=fabs(res->KL_mod-res->KL_reg);
	if(SCORE_CV==0){ya=-symm;}else{ya=Cv;}
	float La[2]; for(n=0; n<nmod; n++)La[n]=res->Lambda[n]; 

	fprintf(file_out, "%.4g\t%.4g\t%.4g\t%.4g\t%.4g\t%.4g",
		xa*REG_COEF,Cv,res->lik_MSA,-res->score,symm,res->Lambda[0]);
	if(exp2)fprintf(file_out, "\t%.4g",res->Lambda[1]);
	fprintf(file_out, "\n");
	sprintf(out, "# reg_par= %.5g score= %.3g Lambda_end= %.3g",
		xa*REG_COEF, ya, res->Lambda[0]);
	if(exp2){
	  sprintf(tmp, "\t%.4g",res->Lambda[1]); strcat(out, tmp);
	}
	fprintf(file_summ, "%s\n", out); printf("%s\n", out);

	float xb=0.5*(x2+x3), yb;
	Cv=Compute_Cv(res, xb, reg_step, w_max,f_aa,
		      P_ia,exp1,exp2,P_mut_a, f_reg_ia,f_msa_ia,wi,
		      E_wt,C_nat,i_sec, name_file, file_summ, label);
	symm=fabs(res->KL_mod-res->KL_reg);
	if(SCORE_CV==0){yb=-symm;}else{yb=Cv;}
	float Lb[2]; for(n=0; n<nmod; n++)Lb[n]=res->Lambda[n]; 

	fprintf(file_out, "%.4g\t%.4g\t%.4g\t%.4g\t%.4g\t%.4g",
		xb*REG_COEF,Cv,res->lik_MSA,-res->score,symm,res->Lambda[0]);
	if(exp2)fprintf(file_out, "\t%.4g",res->Lambda[1]);
	fprintf(file_out, "\n");
	sprintf(out, "# reg_par= %.5g score= %.3g Lambda_end= %.3g",
		xb*REG_COEF, yb, res->Lambda[0]);
	if(exp2){
	  sprintf(tmp, "\t%.4g",res->Lambda[1]); strcat(out, tmp);
	}
	fprintf(file_summ, "%s\n", out); printf("%s\n", out);

	if(ya > yb && ya > y2){
	  x3=x2; x2=xa; y2=ya; // max at xa
	  for(n=0; n<nmod; n++)L_opt[n]=La[n];
	}else if(yb > ya && yb > y2){
	  x1=x2; x2=xb; y2=yb; // max at xb
	  for(n=0; n<nmod; n++)L_opt[n]=Lb[n];
	}else{
	  x1=xa; x3=xb; // max at x2
	}
	if(y2>score_opt &&
	   L_opt[0]>Lambda_thr[0] && L_opt[1]>=Lambda_thr[1]){
	  score_opt=y2; reg_opt=x2;
	}
      }
    } // end bisection
  }

  if(SCORE_CV==0 || reg_opt>=0){
    sprintf(out,"# optimal regularization parameter: %.3g",REG_COEF*reg_opt);
    for(n=0; n<nmod; n++)if(L_opt[n]>0)Lambda_start[n]=L_opt[n];
  }else{
    sprintf(out,"# WARNING, optimal regularization parameter");
    sprintf(tmp," could not be determined, using default %.3g\n#",
	    REG_COEF*reg_ini);
    strcat(out, tmp);
    reg_opt=reg_ini;
  }

  sprintf(tmp," initial value: %.4g\n", REG_FACT); strcat(out, tmp);
  fprintf(file_summ,"%s",out); printf("%s",out); fprintf(file_out,"%s",out);
  Compute_score_reg(res, reg_opt, w_max, f_aa, P_ia, exp1, exp2, P_mut_a,
		    f_reg_ia,f_msa_ia,wi, E_wt, C_nat, i_sec,
		    name_file, file_summ, label);
  float symm=fabs(res->KL_mod-res->KL_reg), Cv=-1;
  if(SCORE_CV==0){score_opt=symm;}else{Cv=score_opt;}
  *opt_res=*res;
  for(n=0; n<nmod; n++)L_opt[n]=res->Lambda[n];

  fprintf(file_out, "%.4g\t%.4g\t%.4g\t%.4g\t%.4g\t%.4g",
	  reg_opt*REG_COEF, Cv, res->lik_MSA,
	  res->KL_mod+res->KL_reg, symm, res->Lambda[0]);
  if(exp2)fprintf(file_out, "\t%.4g",res->Lambda[1]);
  fprintf(file_out, "\n");

  sprintf(out,"# score_max= %.4g\n", score_opt);
  sprintf(tmp,"# optimal Lambda= %.4g %.4g starting: %.4g %.4g\n",
	  L_opt[0], L_opt[1], Lambda_start[0], Lambda_start[1]);
  strcat(out, tmp);
  fprintf(file_summ,"%s",out); printf("%s",out);
  fprintf(file_out,"%s",out); fclose(file_out);

  int zero=Compute_P_WT(P_ia, L_opt, exp1, exp2, P_mut_a, E_wt->L, PMIN);
  printf("%d probability values over %d were < %.2g\n",
	 zero, E_wt->L*20, PMIN);
  Copy_P(P_opt_ia, P_ia, E_wt->L, 20);
  return(reg_opt);
}

double Compute_score_reg(struct MF_results *res, float reg,
			 float w_max, float *f_aa, float **P_ia,
			 float **exp1, float **exp2, float *P_mut_a,
			 float **f_reg_ia, float **f_msa_ia, float *wi,
			 struct REM *E_wt, int **C_nat, int *i_sec,
			 char *name_file, FILE *file_summ, char *label)
{
  float score; int zero; char out[200], tmp[80];
  int n, nmod=1; if(exp2)nmod=2;
  if(UPDATE_LAMBDA)
    for(n=0; n<nmod; n++){
      if(res->Lambda[n]>0)Lambda_start[n]=res->Lambda[n];
    }
  Regularize(f_reg_ia, f_msa_ia, f_aa, E_wt->L, w_max, reg);
  zero=Optimize_distr(res, P_ia, exp1, exp2, P_mut_a, f_reg_ia, f_msa_ia, wi,
		      E_wt, C_nat, i_sec, name_file,file_summ,label,ini_CV);
  score=res->KL_mod+res->KL_reg;
  sprintf(out,
	  "# reg= %.4g zero=%d lik= %.3g KL_mod+KL_reg= %.3g Lambda= %.3g",
	  reg*REG_COEF, zero, res->lik_MSA, score, res->Lambda[0]);
  if(nmod==2){
    sprintf(tmp, " %.3g", res->Lambda[1]); strcat(out, tmp);
  }
  fprintf(file_summ, "%s\n", out); printf("%s\n", out);
  return(score);
}

double Compute_Cv(struct MF_results *SSC_res, float reg, float step, 
		  float w_max, float *f_aa, float **P_SSC_ia,
		  float **exp1, float **exp2, float *P_mut_a,
		  float **f_reg_ia, float **f_msa_ia, float *wi,
		  struct REM *E_wt, int **C_nat, int *i_sec,
		  char *name_file, FILE *file_summ, char *label)
{
  double reg1=reg-step, reg2=reg+step;
  int n, nmod=1; if(exp2)nmod=2;
  if(UPDATE_LAMBDA)
    for(n=0; n<nmod; n++){
      if(SSC_res->Lambda[n]>0)Lambda_start[n]=SSC_res->Lambda[n];
    }
  Regularize(f_reg_ia, f_msa_ia, f_aa, E_wt->L, w_max, reg1);
  Optimize_distr(SSC_res,P_SSC_ia,exp1, exp2,P_mut_a,f_reg_ia,f_msa_ia,wi,
		 E_wt, C_nat, i_sec, name_file, file_summ, label, ini_CV);
  double E1=SSC_res->lik_MSA, KL_mod=SSC_res->KL_mod, KL_reg=SSC_res->KL_reg;
  float Lambda[2]; for(n=0; n<nmod; n++)Lambda[n]=SSC_res->Lambda[n];

  Regularize(f_reg_ia, f_msa_ia, f_aa, E_wt->L, w_max, reg2);
  Optimize_distr(SSC_res,P_SSC_ia,exp1,exp2,P_mut_a,f_reg_ia,f_msa_ia,wi,
		 E_wt, C_nat, i_sec, name_file, file_summ, label, ini_CV);
  double Cv=(E1-SSC_res->lik_MSA)/(step*2);
  SSC_res->lik_MSA=(SSC_res->lik_MSA+E1)/2;
  SSC_res->KL_mod=(SSC_res->KL_mod+KL_mod)/2;
  SSC_res->KL_reg=(SSC_res->KL_reg+KL_reg)/2;
  for(n=0; n<nmod; n++)SSC_res->Lambda[n]=(SSC_res->Lambda[n]+Lambda[n])/2;

  /*char output[200];
  sprintf(output,
	  "# reg_par= %.5g dlik/dreg= %.3g Lambda_start= %.4g zero=%d\n",
	  reg*REG_COEF, Cv, Lambda_start[0], zero);
	  printf("%s", output); //fprintf(file_summ, "%s", output); */

  return(Cv);
}


float Normalize_exponent(float **SSC_exp, int L, char *model){
  int i, a; double sum=0;
  for(i=0; i<L; i++){
    float *SSC=SSC_exp[i], min_SSC=SSC[0];
    for(a=1; a<20; a++)if(SSC[a]<min_SSC)min_SSC=SSC[a];
    for(a=0; a<20; a++){SSC[a]-=min_SSC; sum+=SSC[a];}
  }
  if(NORMALIZE==0)return(0);

  sum/=L; if(sum<0)sum=-sum; 
  printf("Average exponent %s model: %.3g\n", model, sum);
  if(isnan(sum) || sum<=0){
    for(i=0; i<L; i++){
      for(a=0; a<20; a++)if(SSC_exp[i][a]<0 || isnan(SSC_exp[i][a]))break;
      if(a<20){
	printf("i= %d Q: ", i);
	for(a=0; a<20; a++)printf(" %.2g", SSC_exp[i][a]);
	printf("\n");
      }
    }
    return(-1);
  }
  float max_exp=0;
  for(i=0; i<L; i++){
    float *SSC=SSC_exp[i];
    for(a=0; a<20; a++){
      SSC[a]/=sum; if(SSC[a]>max_exp)max_exp=SSC[a];
    }
  }
  return(max_exp);
}
