/*Copyright (C) 2016  SuperDARN Canada

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

/*
ACF Processing main functions

Keith Kotyk
ISAS
July 2015

*/

#include "preprocessing.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/********************LIST NODE STUFF*********************/

/**
Initializes a new range node and returns a pointer to it.
*/
RANGENODE* new_range_node(int range, FITPRMS *fit_prms){
	RANGENODE* new_node;
	new_node = malloc(sizeof(RANGENODE));
	new_node->range = range;
	new_node->CRI = calloc(fit_prms->mppul,sizeof(*new_node->CRI));
	new_node->refrc_idx = 1;
	new_node->alpha_2 = NULL;
	new_node->phases = NULL;
	new_node->pwrs = NULL;
	new_node->elev = NULL;
	new_node->l_pwr_fit = new_fit_data();
 	new_node->q_pwr_fit = new_fit_data();
 	new_node->l_pwr_fit_err = new_fit_data();
 	new_node->q_pwr_fit_err = new_fit_data();
	new_node->phase_fit = new_fit_data();
	new_node->elev_fit = new_fit_data();
	return new_node;

}

/**
Initializes a new pwr node and returns a pointer to it. Returns a null 
pointer if the power at this node is below the fluctuation level.
*/
PWRNODE* new_pwr_node(int range, double alpha_2, LAGNODE* lag, FITPRMS *fit_prms){
	PWRNODE* new_pwr_node;
	double R,R_norm,R_norm_2,real,imag,inverse_alpha_2;



	real = fit_prms->acfd[range * fit_prms->mplgs + lag->lag_idx][0];
	imag = fit_prms->acfd[range * fit_prms->mplgs + lag->lag_idx][1];
	R = sqrt(real * real + imag * imag);

	if(R <= 0.0) return NULL;
	R_norm = R/fit_prms->pwr0[range];
	R_norm_2 = R_norm * R_norm;

	inverse_alpha_2 = 1 / alpha_2;

	new_pwr_node = malloc(sizeof(*new_pwr_node));

	new_pwr_node->lag_idx = lag->lag_idx;
	new_pwr_node->ln_pwr = log(R);
	new_pwr_node->sigma = fit_prms->pwr0[range] * sqrt((R_norm_2 + inverse_alpha_2)/(2 * fit_prms->nave)); 
	new_pwr_node->t = lag->lag_num * fit_prms->mpinc * 1.0e-6;

	return new_pwr_node;

}


/**
Initializes a new phase node and returns a pointer to it.
*/
PHASENODE* new_phase_node(int range, double alpha_2, LAGNODE* lag, FITPRMS *fit_prms){
	PHASENODE* new_phase_node;
	double real,imag;

	new_phase_node = malloc(sizeof(*new_phase_node));

	real = fit_prms->acfd[range * fit_prms->mplgs + lag->lag_idx][0];
	imag = fit_prms->acfd[range * fit_prms->mplgs + lag->lag_idx][1];

	new_phase_node->lag_idx = lag->lag_idx;
	new_phase_node->phi = atan2(imag,real);

	/*This is merely a placeholder for alpha right now. This gets changed in the
	later calculation of sigma after the power fit */
	new_phase_node->sigma = alpha_2;

	new_phase_node->t = lag->lag_num * fit_prms->mpinc * 1.0e-6;

	return new_phase_node;

}

/**
Initializes a new elevation phase node and returns a pointer to it.
*/
PHASENODE* new_elev_node(int range, double alpha_2, LAGNODE* lag, FITPRMS *fit_prms){
		PHASENODE* new_elev_node;
		double real,imag;

		new_elev_node = malloc(sizeof(*new_elev_node));

		real = fit_prms->xcfd[range * fit_prms->mplgs + lag->lag_idx][0];
		imag = fit_prms->xcfd[range * fit_prms->mplgs + lag->lag_idx][1];

		new_elev_node->lag_idx = lag->lag_idx;
		new_elev_node->phi = atan2(imag,real);
		
		/*This is merely a placeholder for alpha right now. This gets changed in the
		later calculation of sigma after the power fit */
		new_elev_node->sigma = alpha_2;

		new_elev_node->t = lag->lag_num * fit_prms->mpinc * 1.0e-6;

		return new_elev_node;

}

/**
Frees all memory for range data
*/
void free_range_node(llist_node range){
	RANGENODE* range_node;

	range_node = (RANGENODE*) range;

	if(range_node->CRI !=NULL){
		free(range_node->CRI);
	}

	if(range_node->alpha_2 != NULL){
		llist_destroy(range_node->alpha_2,TRUE,free);
	}

	if(range_node->phases != NULL){
		llist_destroy(range_node->phases,TRUE,free);
	}

	if(range_node->pwrs != NULL){
		llist_destroy(range_node->pwrs,TRUE,free);
	}

	if(range_node->elev != NULL){
		llist_destroy(range_node->elev,TRUE,free);
	}

	if(range_node->l_pwr_fit != NULL){
		free_fit_data(range_node->l_pwr_fit);
	}

	if(range_node->q_pwr_fit != NULL){
		free_fit_data(range_node->q_pwr_fit);
	}

	if(range_node->l_pwr_fit_err != NULL){
		free_fit_data(range_node->l_pwr_fit_err);
	}

	if(range_node->q_pwr_fit_err != NULL){
		free_fit_data(range_node->q_pwr_fit_err);
	}	

	if(range_node->phase_fit != NULL){
		free_fit_data(range_node->phase_fit);
	}

	if(range_node->elev_fit != NULL){
		free_fit_data(range_node->elev_fit);
	}

	if(range_node != NULL){
		free(range_node);
	}


}








/*++++++++++++++++++++CALLBACKS FOR COMPARISONS+++++++++++++++++++++*/

/**
Callback for comparing two doubles in qsort
*/
int dbl_cmp(const void *x,const void *y) {
  double *a,*b;
  a=(double *) x;
  b=(double *) y;
  if (*a > *b) return 1;
  else if (*a == *b) return 0;
  else return -1;
}

/**
Callback used to compare two lags 
*/
int compare_lags(llist_node first,llist_node second){
	LAGNODE* lag1;
	LAGNODE* lag2;
	lag1 = (LAGNODE*)(first);
	lag2 = (LAGNODE*)(second);

	if(lag1->lag_num < lag2->lag_num){
		return -1;
	}
	else if(lag1->lag_num == lag2->lag_num){
		return 0;
	}
	else{
		return 1;
	}
}

/**
Callback used to identify a pwr node by its lag number
*/
bool pwr_node_eq(llist_node node,llist_node cmp){
	PWRNODE* pwr_node;
	int value;

	pwr_node = (PWRNODE*) node;
	value = *(int*)(cmp);

	return (pwr_node->lag_idx == value);

}

/**
Callback used to identify a phase node by its lag number
*/
bool phase_node_eq(llist_node node,llist_node cmp){
	PHASENODE* phase_node;
	int value;

	phase_node = (PHASENODE*) node;
	value = *(int*)(cmp);

	return (phase_node->lag_idx == value);

}

/**
Callback used to compares doubles within a list
*/
int compare_doubles(llist_node first,llist_node second){
	if(*(double*)(first) < *(double*)(second)){
		return -1;
	}
	else if(*(double*)(first) < *(double*)(second)){
		return 0;
	}
	else{
		return 1;
	}
}

/**
Callback used to compare samples
*/
bool sample_node_eq(llist_node node,llist_node cmp){
	int smp;
	int value;

	smp = *(int*)(node);
	value = *(int*)(cmp);

	return (smp == value);

}

/**
Callback used to compare ranges
*/
bool range_node_eq(llist_node node,llist_node cmp){
	RANGENODE* range_node;
	int value;

	range_node = (RANGENODE*) node;
	value = *(int*)(cmp);

	return (range_node->range == value);

}

/**
Callback used to compare alpha
*/
bool alpha_node_eq(llist_node node,llist_node cmp){
	double alpha_node;
	double value;

	alpha_node = *(double*) node;
	value = *(double*)(cmp);

	return (alpha_node == value);

}










/*++++++++++++++++++++PRINT CALLBACKS FOR DIFFERENT NODES+++++++++++++++++++*/


void print_node(llist_node node){
	printf("sample %d\n",*(int*)(node));
}

void print_alpha_node(llist_node node, FILE* fp){
	fprintf(fp,"%f ",*(double*)(node));
}

void print_lag_node(llist_node node){
	LAGNODE* lag;
	lag = (LAGNODE*)(node);
	printf("lag %d %d %d %d\n",lag->lag_idx,lag->lag_num,lag->pulses[0],lag->pulses[1]);

}

void print_pwr_node(llist_node node,FILE* fp){
	PWRNODE* pwr;
	pwr = (PWRNODE*)(node);
	fprintf(fp,"ln_pwr %f sigma %f t %f\n",pwr->ln_pwr,pwr->sigma,pwr->t);

}

void print_phase_node(llist_node node, FILE* fp){
	PHASENODE* phi;
	phi = (PHASENODE*)(node);
	fprintf(fp,"phi %f sigma %f t %f\n",phi->phi,phi->sigma,phi->t);

}

void print_range_node(llist_node node){
	int i;
	FILE* fp;

	fp = fopen("fullrangeinfo.txt","a");
	fprintf(fp,"range %d\n",((RANGENODE*)node)->range);

	fprintf(fp,"CRI ");
	for(i=0;i<8;i++){
		fprintf(fp,"%f ",((RANGENODE*)node)->CRI[i]);
	}
	fprintf(fp,"\nALPHA\n");

	llist_for_each_arg(((RANGENODE*)node)->alpha_2, (node_func_arg)print_alpha_node,fp,NULL);
	fprintf(fp,"\n");
	llist_for_each_arg(((RANGENODE*)node)->pwrs, (node_func_arg)print_pwr_node,fp,NULL);
	fprintf(fp,"LINEAR POWER FIT\n");
	print_fit_data(((RANGENODE*)node)->l_pwr_fit,fp);
	fprintf(fp,"QUADRATIC POWER FIT\n");
	print_fit_data(((RANGENODE*)node)->q_pwr_fit,fp);
	fprintf(fp,"ACF_PHASE FIT\n");
	llist_for_each_arg(((RANGENODE*)node)->phases, (node_func_arg)print_phase_node,fp,NULL);
	print_fit_data(((RANGENODE*)node)->phase_fit,fp);
	fprintf(fp,"XCF_PHASE FIT\n");
	print_fit_data(((RANGENODE*)node)->elev_fit,fp);
	llist_for_each_arg(((RANGENODE*)node)->elev, (node_func_arg)print_phase_node,fp,NULL);
	fclose(fp);
	
}

void print_uncorrected_phase(llist_node node){
  llist_for_each(((RANGENODE*)node)->phases, (node_func)print_phase_node_to_file);
}

void print_phase_node_to_file(llist_node node){
	PHASENODE* phi;
	FILE *fp;
	phi = (PHASENODE*)(node);
	fp = fopen("phases.txt","a");
	fprintf(fp,"%f\n",phi->phi);
	fclose(fp);

}






/*++++++++++++++++++++++VARIOUS HELPER FUNCTIONS+++++++++++++++++++*/

/**
Helper callback function that calculates the alpha value for each lag
and fills a list of these values. Uses formulas from Bendat and Piersol.
*/
void calculate_alpha_at_lags(llist_node lag, llist_node range, double* lag_0_pwr){
	double pulse_i_cri,pulse_j_cri;
	double* alpha_2;
	LAGNODE* lag_node;
	RANGENODE* range_node;
	double pwr;

	pwr=*lag_0_pwr;

	lag_node = (LAGNODE*)lag;
	range_node = (RANGENODE*)range;

	pulse_i_cri = range_node->CRI[lag_node->pulses[0]];
	pulse_j_cri = range_node->CRI[lag_node->pulses[1]];

	alpha_2 = malloc(sizeof(*alpha_2));
	*alpha_2 = (pwr * pwr)/((pwr + pulse_i_cri) * (pwr + pulse_j_cri));
	llist_add_node(range_node->alpha_2,(llist_node)alpha_2,0);

}

/**
This function finds the bad samples that are blacked out by TX overlap
*/
void mark_bad_samples(FITPRMS *fit_prms, llist bad_samples){
	int i, sample;
	long ts, t1=0, t2=0;
	int* bad_sample;
	i = -1;
	ts = (long) fit_prms->lagfr;
	sample = 0;

	t2 = 0L;


	while (i < (fit_prms->mppul - 1)) {
	/* first, skip over any pulses that occur before the first sample
	   defines transmitter pulse blanket window*/

		while ((ts > t2) && (i < (fit_prms->mppul - 1))) {
			i++;
			t1 = (long) (fit_prms->pulse[i]) * (long) (fit_prms->mpinc)
			- fit_prms->txpl/2;
	  		t2 = t1 + 3*fit_prms->txpl/2 + 100; /* adjust for rx-on delay */
		}	

	/*	we now have a pulse that occurs after the current sample.  Start
		incrementing the sample number until we find a sample that lies
		within the pulse */

		while (ts < t1) {
			sample++;
			ts = ts + fit_prms->smsep;
		}

	/*	ok, we now have a sample which occurs after the pulse starts.
		check to see if it occurs before the pulse ends, and if so, mark
		it as a bad sample */
		while ((ts >= t1) && (ts <= t2)) {
			bad_sample = malloc(sizeof(int));
			*bad_sample = sample;
			if(llist_add_node(bad_samples,(llist_node)bad_sample,0) != LLIST_SUCCESS){
				fprintf(stderr,"list node failed to add in Mark_Bad_Samples\n");
			};
			sample++;
			ts = ts + fit_prms->smsep;
		}
	}
}

/**
Helper callback function to remove phase and pwr lags that are marked by
bad samples due to TX overlap.
*/
void filter_tx_overlapped_lags(llist_node range, llist lags, llist bad_samples){
	RANGENODE* range_node;
	double* alpha_2;
	LAGNODE* lag;

	int smp1, smp2;

	range_node = (RANGENODE*) range;

	llist_reset_iter(lags);
	llist_reset_iter(range_node->alpha_2);

	do{
		llist_get_iter(lags,(void**)&lag);
		smp1 = lag->sample_base1 + range_node->range;
		smp2 = lag->sample_base2 + range_node->range;

		if((llist_find_node(bad_samples,&smp1,NULL) == LLIST_SUCCESS) ||
		   (llist_find_node(bad_samples,&smp2,NULL) == LLIST_SUCCESS)){

			llist_delete_node(range_node->pwrs,&lag->lag_idx,TRUE,free);
			llist_delete_node(range_node->phases,&lag->lag_idx,TRUE,free);
			llist_delete_node(range_node->elev,&lag->lag_idx,TRUE,free);

			llist_get_iter(range_node->alpha_2,(void**)&alpha_2);
			/*We check to see if we remove the head of the list. If we do,
		   	  we do not want to move the cursor as it will already be placed on
		   	  the correct node after the head of the list is deleted*/
			if(alpha_2 == llist_peek(range_node->alpha_2)){
				llist_delete_node(range_node->alpha_2,alpha_2,TRUE,free);
				continue;
			}
			else{
				llist_delete_node(range_node->alpha_2,alpha_2,TRUE,free);
			}
			
		}
		llist_go_next(range_node->alpha_2);

	}while(llist_go_next(lags) != LLIST_END_OF_LIST);

}


/**
Helper callback function which performs the phase correction by determining
how many 2*PI corrections need to be added to each phase value using the
slope estimate.
*/
void phase_correction(llist_node phase, double* slope_est){
	PHASENODE* phase_node;
	double phi_pred;
	double phi_corr;

	phase_node = (PHASENODE*) phase;

	phi_pred = *slope_est * phase_node->t;

	phi_corr = round((phi_pred - phase_node->phi)/(2 * M_PI));

	phase_node->phi += phi_corr * 2 * M_PI;

}

/**
This function determines the minimum power level for which an ACF is pure
noise. This is used to filter bad ACFs.
*/
double ACF_cutoff_pwr(FITPRMS *fit_prms){

    int i;
    double min_pwr = 0.0;
    double *pwr_levels;
    int ni = 0;

    pwr_levels = malloc(fit_prms->nrang * sizeof(double));

    if(pwr_levels == NULL){
        return -1.0;
    }

    /* Determine the lag_0 noise level (0 dB reference) and the noise level at 
     which fit_acf is to quit (average power in the 
     fluctuations of the acfs which are pure noise) */

    for (i=0; i < fit_prms->nrang; i++) {
        /* transfer powers into local array */
        pwr_levels[i] = fit_prms->pwr0[i];
    }

    qsort(pwr_levels, fit_prms->nrang, sizeof(double), dbl_cmp);
    /* determine the average lag0 power of the 10 lowest power acfs */
    
    
    i=0;

    
    /*  look for the lowest 10 values of lag0 power and average to 
            get the noise level.  Ignore values that are exactly 0.  If
            you can't find 10 useable values within the first 1/3 of the
            sorted power list, then just use whatever you got in that 
            first 1/3.  If you didn't get any useable values, then use
            the NOISE parameter */
        
    while ((ni < 10) && (i < fit_prms->nrang/3)) {
        if (pwr_levels[i]) ++ni;
        min_pwr += pwr_levels[i++];  
    }

    ni = (ni > 0) ? ni :  1;
    min_pwr = min_pwr/ni;
    if (min_pwr < 1.0) min_pwr = fit_prms->noise;

    free(pwr_levels);
    return min_pwr;

}





/*+++++++++++++++++++MAIN ACF PROCESSING FUNCTIONS+++++++++++++++++++*/


/**
Removes ACFs entirely from analysis if they are deemed to be pure noise
*/
void Filter_Bad_ACFs(FITPRMS *fit_prms, llist ranges, double noise_pwr){
	int i=0;
	RANGENODE* range_node;
	double cutoff_pwr;


	cutoff_pwr = ACF_SNR_CUTOFF * noise_pwr * (1 + 1/sqrt(fit_prms->nave));

	llist_reset_iter(ranges);
	

	for(i=0;i<fit_prms->nrang;i++){
		llist_get_iter(ranges,(void**)&range_node);
		if((fit_prms->pwr0[i] <= cutoff_pwr) || (llist_size(range_node->pwrs) < MIN_LAGS)){
		   	/*We check to see if we remove the head of the list. If we do,
		   	  we do not want to move the cursor as it will already be placed on
		   	  the correct node after the head of the list is deleted*/
		   	if(range_node == llist_peek(ranges)){
				llist_delete_node(ranges,&range_node->range,TRUE,free_range_node);
				continue;
			}
			else{
				llist_delete_node(ranges,&range_node->range,TRUE,free_range_node);
			}
		}
		llist_go_next(ranges);

	}

}

/**
Fills the list of ranges with new range nodes.
*/
void Fill_Range_List(FITPRMS *fit_prms, llist ranges){
	int i;
	RANGENODE* temp;

	for(i=0; i<fit_prms->nrang; i++){
	    if(fit_prms->pwr0[i] != 0.0){
		temp = new_range_node(i,fit_prms);
		llist_add_node(ranges,(llist_node)temp,0);
	    }
	}
}

/**
Prunes off low power lags determined by cutoff criteria. Once a
cutoff lag is determined, all subsequent lags in the list are 
removed.
*/
void Filter_Low_Pwr_Lags(llist_node range, FITPRMS* fit_prms){
	double log_sigma_fluc;
	double* alpha_2;
	PWRNODE* pwr_node;
	RANGENODE* range_node;
	int cutoff_lag = fit_prms->mplgs + 1;

	range_node = (RANGENODE*) range;

	/*Cutoff fluctuation level for filtering*/
	log_sigma_fluc =  log(FLUCTUATION_CUTOFF_COEFF * fit_prms->pwr0[range_node->range]/sqrt(2 * fit_prms->nave));


	llist_reset_iter(range_node->pwrs);
	llist_reset_iter(range_node->alpha_2);


	do{
		llist_get_iter(range_node->pwrs,(void**)&pwr_node);
		llist_get_iter(range_node->alpha_2,(void**)&alpha_2);

		if(pwr_node->lag_idx > cutoff_lag){
			llist_delete_node(range_node->pwrs,&pwr_node->lag_idx,TRUE,free);
		}
		else{
			/*Full cutoff criteria for lag filtering*/

			if((1/ sqrt(*alpha_2) <= ALPHA_CUTOFF) && (pwr_node->ln_pwr <= log_sigma_fluc)){
				cutoff_lag = pwr_node->lag_idx;
				llist_delete_node(range_node->pwrs,&pwr_node->lag_idx,TRUE,free);
			}
		}

		llist_go_next(range_node->alpha_2);
	}while(llist_go_next(range_node->pwrs) != LLIST_END_OF_LIST);


}

/**
For a given range, this function finds the cross-range interference of
of each pulse in the pulse sequence
*/
void Find_CRI(llist_node node,FITPRMS *fit_prms){
	int pulse_to_check;
	int pulse;
	int range_to_check;
	double total_cri;
	int tau;
	int diff_pulse;

	RANGENODE* range_node = (RANGENODE*)node;

	if (fit_prms->smsep != 0) {
		tau = fit_prms->mpinc / fit_prms->smsep;
	} else {
		fprintf( stderr, "r_overlap: WARNING, using txpl instead of smsep...\n");
		tau = fit_prms->mpinc / fit_prms->txpl;
	}

	for (pulse_to_check = 0;  pulse_to_check < fit_prms->mppul; ++pulse_to_check) {
		total_cri = 0.0;

		for (pulse = 0; pulse < fit_prms->mppul; ++pulse) {
      		diff_pulse = fit_prms->pulse[pulse_to_check] - fit_prms->pulse[pulse];
			range_to_check = diff_pulse * tau + range_node->range;

			if ((pulse != pulse_to_check) &&
				(0 <= range_to_check) && 
				(range_to_check < fit_prms->nrang)) {
					total_cri += fit_prms->pwr0[range_to_check];
			}
		} 

		range_node->CRI[pulse_to_check] = total_cri;
	}           
  
}



/**
For a given range, this function creates a new list of alpha values at
each lag for that range
*/
void Find_Alpha(llist_node range,llist lags, FITPRMS *fit_prms){
	llist alpha_2;
	RANGENODE* range_node;
	double lag_0_pwr;

	range_node = (RANGENODE*)range;

	alpha_2 = llist_create(compare_doubles,alpha_node_eq,0);

	range_node->alpha_2 = alpha_2;

	lag_0_pwr = fit_prms->pwr0[range_node->range];

	llist_for_each_arg(lags,(node_func_arg)calculate_alpha_at_lags,range_node,&lag_0_pwr);

}



/**
For a given range, this function performs a 2 interation process to unwrap
the phase for fitting.
*/
void ACF_Phase_Unwrap(llist_node range){
	RANGENODE* range_node;
	PHASENODE* phase_curr;
	PHASENODE* phase_prev;

	double d_phi,sigma_bar,d_tau;
	double slope_est = 0.0 ,slope_denom = 0.0,slope_num = 0.0;
	double S_xy, S_xx;

	range_node = (RANGENODE*) range;

	llist_reset_iter(range_node->phases);

	llist_get_iter(range_node->phases, (void**)&phase_curr);

	/*First iteration to achieve a slope estimate using a weighted 
	average of the slope between adjacent phases*/
	while(llist_go_next(range_node->phases) != LLIST_END_OF_LIST){
		phase_prev = phase_curr;
		llist_get_iter(range_node->phases, (void**)&phase_curr);

		d_phi = phase_curr->phi - phase_prev->phi;

		sigma_bar = (phase_curr->sigma + phase_prev->sigma)/2;

		d_tau = phase_curr->t - phase_prev->t;

		if(fabs(d_phi) < M_PI){
			slope_num += d_phi/(sigma_bar * sigma_bar)/d_tau;
			slope_denom += 1/(sigma_bar * sigma_bar);
		}

	}

	slope_est = slope_num / slope_denom;

	llist_for_each_arg(range_node->phases,(node_func_arg)phase_correction,&slope_est,NULL);

	llist_reset_iter(range_node->phases);
	S_xx = 0;
	S_xy = 0;

	/*Second iteration to improve slope estimate by quickly fitting to first round unwrap*/
	do{

		llist_get_iter(range_node->phases, (void**)&phase_curr);

		if(phase_curr->sigma > 0){
			S_xy += (phase_curr->phi * phase_curr->t)/(phase_curr->sigma * phase_curr->sigma);

			S_xx += (phase_curr->t * phase_curr->t)/(phase_curr->sigma * phase_curr->sigma);
		}

	}while(llist_go_next(range_node->phases) != LLIST_END_OF_LIST);


	 slope_est = S_xy / S_xx;

	llist_for_each_arg(range_node->phases,(node_func_arg)phase_correction,&slope_est,NULL);

}

/**
This function unwraps the phase for the XCF.We only need one round of improving the 
unwrap because we are using the fitted slope of the acf phase as our initial phase 
unwrapping guess.
*/
void XCF_Phase_Unwrap(llist_node range){
	RANGENODE* range_node;
	PHASENODE* phase_curr;
	double S_xy, S_xx,slope_est;

	range_node = (RANGENODE*) range;

	llist_for_each_arg(range_node->elev,(node_func_arg)phase_correction,&range_node->phase_fit->b,NULL);

	llist_reset_iter(range_node->elev);
	S_xx = 0;
	S_xy = 0;

	/*one iteration to improve slope estimate by quickly fitting to first round unwrap*/
	do{

		llist_get_iter(range_node->elev, (void**)&phase_curr);

		if(phase_curr->sigma > 0){
			S_xy += (phase_curr->phi * phase_curr->t)/(phase_curr->sigma * phase_curr->sigma);

			S_xx += (phase_curr->t * phase_curr->t)/(phase_curr->sigma * phase_curr->sigma);
		}

	}while(llist_go_next(range_node->elev) != LLIST_END_OF_LIST);


	 slope_est = S_xy / S_xx;

	llist_for_each_arg(range_node->elev,(node_func_arg)phase_correction,&slope_est,NULL);
}


/**
This function filters out bad phase and pwr lags due to TX overlap and
filters out pwr lags that are below a fluctuation level
*/
void Filter_TX_Overlap(llist ranges, llist lags, FITPRMS *fit_prms){
	llist bad_samples = NULL;

	bad_samples = llist_create(NULL,sample_node_eq,0);

	mark_bad_samples(fit_prms,bad_samples);

	llist_for_each_arg(ranges, (node_func_arg)filter_tx_overlapped_lags, lags, bad_samples);

	llist_destroy(bad_samples,TRUE,free);

}


/**
The commented out code can take a given pulse sequence, and create a sorted list of lags
that can be formed from combining pulses. This algorithm doesnt need anything more
than the pulse sequence and can be used for custom pulse sequences or if no lag table
exists. The uncommented code determines lags from the lag table supplied in the raw
data.
*/
void Determine_Lags(llist lags,FITPRMS *fit_prms){
	int i, j;
	LAGNODE* temp;

	/****Find lags from pulse sequence****/
/*	for(i=fit_prms->mppul-1;i>=0;i--){
		for(j=i-1;j>=0;j--){
			if((fit_prms->pulse[i] - fit_prms->pulse[j]) <= fit_prms->mplgs){
				temp = malloc(sizeof(LAGNODE));
				temp->lag_num = fit_prms->pulse[i] - fit_prms->pulse[j];
				temp->pulses[0] = j;
				temp->pulses[1] = i;
				temp->sample_base1 = fit_prms->pulse[j] * (fit_prms->mpinc/fit_prms->smsep);
				temp->sample_base2 = fit_prms->pulse[i] * (fit_prms->mpinc/fit_prms->smsep);
				llist_add_node(lags,(llist_node)temp,0);

		    }

		}
	}

	llist_sort(lags,SORT_LIST_ASCENDING);

	temp = malloc(sizeof(LAGNODE));
	temp->lag_num = 0;
	temp->pulses[0] = fit_prms->pulse[0];
	temp->pulses[1] = fit_prms->pulse[0];
	temp->sample_base1 = fit_prms->pulse[0] * (fit_prms->mpinc/fit_prms->smsep);
	temp->sample_base2 = fit_prms->pulse[0] * (fit_prms->mpinc/fit_prms->smsep);
	llist_add_node(lags,(llist_node)temp,ADD_NODE_FRONT);

	llist_reset_iter(lags);
	i=0;
	do{
		llist_get_iter(lags,(void**)&temp);
		temp->lag_idx = i++;
	}while(llist_go_next(lags) != LLIST_END_OF_LIST);*/

	/***Find lags from the lag table****/
	for(i=0;i<fit_prms->mplgs;i++){
		temp = malloc(sizeof(LAGNODE));
		temp->lag_num = fit_prms->lag[1][i] - fit_prms->lag[0][i];
		for(j=0;j<fit_prms->mppul;j++){
			if(fit_prms->pulse[j] == fit_prms->lag[1][i]){
				temp->pulses[1] = j;
			}

			if(fit_prms->pulse[j] == fit_prms->lag[0][i]){
				temp->pulses[0] = j;
			}
		}
		temp->sample_base1 = fit_prms->lag[0][i] * (fit_prms->mpinc/fit_prms->smsep);
		temp->sample_base2 = fit_prms->lag[1][i] * (fit_prms->mpinc/fit_prms->smsep);
		llist_add_node(lags,(llist_node)temp,0);
	}

	llist_reset_iter(lags);
	i=0;
	do{
		llist_get_iter(lags,(void**)&temp);
		temp->lag_idx = i++;
	}while(llist_go_next(lags) != LLIST_END_OF_LIST);

}


/**
For each range, this function creates lists of associated ACF phases, XCF phases and ACF pwrs
for each lag of that range from the raw data.
*/
void Fill_Data_Lists_For_Range(llist_node range,llist lags,FITPRMS *fit_prms){
	RANGENODE* range_node;
	PWRNODE* pwr_node;
	PHASENODE* phase_node;
	PHASENODE* elev_node;
	LAGNODE* lag;
	double* alpha_2;
	int i;

	range_node = (RANGENODE*) range;

	range_node->pwrs = llist_create(compare_doubles,pwr_node_eq,0);;
	range_node->phases = llist_create(compare_doubles,phase_node_eq,0);
	range_node->elev = llist_create(compare_doubles,phase_node_eq,0);

	llist_reset_iter(range_node->alpha_2);
	llist_reset_iter(lags);

	for(i=0;i<fit_prms->mplgs;i++){

		llist_get_iter(range_node->alpha_2,(void**)&alpha_2);
		llist_get_iter(lags,(void**)&lag);

		pwr_node = new_pwr_node(range_node->range,*alpha_2,lag,fit_prms);
		phase_node = new_phase_node(range_node->range,*alpha_2,lag,fit_prms);
		elev_node = new_elev_node(range_node->range,*alpha_2,lag,fit_prms);
		if(pwr_node != NULL){ /* A NULL ptr means the pwr level was too low*/
			llist_add_node(range_node->pwrs,(llist_node)pwr_node,0);
		}
		llist_add_node(range_node->phases,(llist_node)phase_node,0);
		llist_add_node(range_node->elev,(llist_node)elev_node,0);

		llist_go_next(lags);
		llist_go_next(range_node->alpha_2);

	}


}




