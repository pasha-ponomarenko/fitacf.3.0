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

/*Least squares fitting

Keith Kotyk
ISAS
June 16, 2015

Functions follow the naming scheme and procedures used in
NUMERICAL RECIPES IN C: THE ART OF SCIENTIFIC COMPUTING
pages 661-663

*/

#include "leastsquares.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

#define ITMAX 100 
#define EPS 3.0e-7 
#define FPMIN 1.0e-30

typedef struct data{
	double y;
	double x;
	double sigma;
}DATA;


/**
Returns a pointer to a new FITDATA structure
*/
FITDATA *new_fit_data(){
	FITDATA *new_fit_data;
	SUMS *new_sums;

	new_sums = malloc(sizeof(*new_sums));
	new_sums->S = 0.0;
	new_sums->S_x = 0.0;
	new_sums->S_y = 0.0;
	new_sums->S_xx = 0.0;
	new_sums->S_xy = 0.0;

	new_fit_data = malloc(sizeof(*new_fit_data));
	new_fit_data->sums = new_sums;
	new_fit_data->delta = 0.0;
	new_fit_data->a = 0.0;
	new_fit_data->b = 0.0;
	new_fit_data->sigma_2_a = 0.0;
	new_fit_data->sigma_2_b = 0.0;
	new_fit_data->delta_a = 0.0;
	new_fit_data->delta_b = 0.0;	
	new_fit_data->cov_ab = 0.0;
	new_fit_data->r_ab = 0.0;
	new_fit_data->Q = 0.0;
	new_fit_data->chi_2 = 0.0;

	return new_fit_data;

}

void free_fit_data(FITDATA *fit_data){
	if(fit_data->sums != NULL){
		free(fit_data->sums);
	}

	if(fit_data != NULL){
		free(fit_data);
	}
}

/**
prints the contents of a FITDATA structure
*/
void print_fit_data(FITDATA *fit_data, FILE* fp){
	fprintf(fp,"S: %e\n",fit_data->sums->S);
	fprintf(fp,"S_x: %e\n",fit_data->sums->S_x);
	fprintf(fp,"S_y: %e\n",fit_data->sums->S_y);
	fprintf(fp,"S_xx: %e\n",fit_data->sums->S_xx);
	fprintf(fp,"S_xy: %e\n",fit_data->sums->S_xy);
	fprintf(fp,"delta: %e\n",fit_data->delta);
	fprintf(fp,"a: %f\n",fit_data->a);
	fprintf(fp,"b: %f\n",fit_data->b);
	fprintf(fp,"sigma_2_a: %f\n",fit_data->sigma_2_a);
	fprintf(fp,"sigma_2_b: %f\n",fit_data->sigma_2_b);
	fprintf(fp,"delta_a: %f\n",fit_data->delta_a);
	fprintf(fp,"delta_b: %f\n",fit_data->delta_b);
	fprintf(fp,"cov_ab: %f\n",fit_data->cov_ab);
	fprintf(fp,"r_ab: %f\n",fit_data->r_ab);
	fprintf(fp,"Q: %f\n",fit_data->Q);

}

/**
Calculates the sums used in least squares fitting
*/
void calculate_sums(llist_node data,FITDATA *fit_data,FIT_TYPE* fit_type){
	DATA* data_node;
	double x,y,sigma;

	data_node = (DATA*) data;
	x = data_node->x;
	y = data_node->y;
	sigma = data_node->sigma;

	if(sigma <= 0.0) return;

	switch(*fit_type){
		case LINEAR:
			fit_data->sums->S += 1/(sigma * sigma); 
			fit_data->sums->S_x += x/(sigma * sigma);
			fit_data->sums->S_y += y/(sigma * sigma);
			fit_data->sums->S_xx += (x * x)/(sigma * sigma);
			fit_data->sums->S_xy += (x * y)/(sigma * sigma);
			break;
		case QUADRATIC:
			fit_data->sums->S += 1/(sigma * sigma); 
			fit_data->sums->S_x += x * x/(sigma * sigma);
			fit_data->sums->S_y += y/(sigma * sigma);
			fit_data->sums->S_xx += (x * x * x *x)/(sigma * sigma);
			fit_data->sums->S_xy += (x * x * y)/(sigma * sigma);
			break;
	}

}


void find_chi_2(llist_node data,FITDATA *fit_data, FIT_TYPE* fit_type){
	DATA* data_node;
	double x,y,sigma;
	double chi;
	data_node = (DATA*) data;
	x = data_node->x;
	y = data_node->y;
	sigma = data_node->sigma;

	if(sigma <= 0.0) return;

	switch(*fit_type){
		case LINEAR:
			chi = (y - fit_data->a - fit_data->b * x) / sigma;
			fit_data->chi_2 += chi * chi;
			break;
		case QUADRATIC:
			chi = (y - fit_data->a - fit_data->b * x * x) / sigma;
			fit_data->chi_2 += chi * chi;
			break;
		}
}

/**
Computes the log of a gamma function to prevent overflows
Taken from NUMERICAL RECIPES IN C: THE ART OF SCIENTIFIC COMPUTING
*/
double gammln(double xx)
{
	double x,y,tmp,ser;
	static double cof[6]={76.18009172947146,-86.50532032941677,
		24.01409824083091,-1.231739572450155,
		0.1208650973866179e-2,-0.5395239384953e-5};
		int j;
		y=x=xx;
		tmp=x+5.5;
		tmp -= (x+0.5)*log(tmp);
		ser=1.000000000190015;
		for (j=0;j<=5;j++) ser += cof[j]/++y;
			return -tmp+log(2.5066282746310005*ser/x);
}

/**
Computes the gamma function as a series representation
Taken from NUMERICAL RECIPES IN C: THE ART OF SCIENTIFIC COMPUTING
*/
void gamma_series_rep(double *gamser, double a, double x, double *gln)
{
	int n;
	double sum,del,ap;
	*gln=gammln(a);
	if (x <= 0.0) {
		if (x < 0.0) /*fprintf(stderr,"error x = %f\n",x);*/
		*gamser=0.0;
		return;
	} else {
		ap=a;
		del=sum=1.0/a;
		for (n=1;n<=ITMAX;n++) {
			++ap;
			del *= x/ap;
			sum += del;
			if (fabs(del) < fabs(sum)*EPS) {
				*gamser=sum*exp(-x+a*log(x)-(*gln));
				return;
			}
		}
		return;
	}
}

/**
Computes the gamma function as continued fractions
Taken from NUMERICAL RECIPES IN C: THE ART OF SCIENTIFIC COMPUTING
*/
void gamma_continued_frac(double *gammcf, double a, double x, double *gln)
{
	int i;
	double an,b,c,d,del,h;
	*gln=gammln(a);
	b=x+1.0-a; 
	c=1.0/FPMIN;
	d=1.0/b;
	h=d;
	for (i=1;i<=ITMAX;i++) { 
		an = -i*(i-a);
		b += 2.0;
		d=an*d+b;
		if (fabs(d) < FPMIN) d=FPMIN;
		c=b+an/c;
		if (fabs(c) < FPMIN) c=FPMIN;
		d=1.0/d;
		del=d*c;
		h *= del;
		if (fabs(del-1.0) < EPS) break;
	}
	if (i > ITMAX) 
	*gammcf=exp(-x+a*log(x)-(*gln))*h; 
}

/**
Computes the incomplete upper gamma function using either 
series representation or continued fractions, depending on
which will be faster
*/
double gammaq(double a, double x){

	double gamser = 0.0,gammcf = 0.0,gln = 0.0;
	if (x < 0.0 || a <= 0.0) return -1.0;
	if (x < (a+1.0)) { 
		gamma_series_rep(&gamser,a,x,&gln);
		return 1.0-gamser;
	} else { 
		gamma_continued_frac(&gammcf,a,x,&gln);
		return gammcf;
	}
}

/**
Fits data to a two parameter straight line model and provides uncertainty in that fit
*/
void two_param_straight_line_fit(FITDATA *fit_data,llist data,int confidence, int DoF){

	double S,S_x,S_y,S_xx,S_xy;
	double delta_chi_2[6][2] = {{1.00,2.30},
							   	{2.71,4.61},
							   	{4.00,6.17},
							   	{6.63,9.21},
							   	{9.00,11.8},
							   	{15.1,18.4}};
	FIT_TYPE linear = LINEAR;

	confidence -= 1;
	DoF -= 1;
	llist_for_each_arg(data,(node_func_arg)calculate_sums,fit_data,&linear);


	S = fit_data->sums->S;
	S_x = fit_data->sums->S_x;
	S_y = fit_data->sums->S_y;
	S_xx = fit_data->sums->S_xx;
	S_xy = fit_data->sums->S_xy;

	fit_data->delta = S*S_xx - S_x*S_x;
	fit_data->a = (S_xx*S_y - S_x*S_xy)/fit_data->delta;
	fit_data->b = (S*S_xy - S_x*S_y)/fit_data->delta;
	fit_data->sigma_2_a = S_xx/fit_data->delta;
	fit_data->sigma_2_b = S/fit_data->delta;
	fit_data->cov_ab = -1 * S_x/fit_data->delta;
	fit_data->r_ab = -1 * S_x/sqrt(S * S_xx);

	fit_data->delta_a = sqrt(delta_chi_2[confidence][DoF]) * sqrt(fit_data->sigma_2_a);
	fit_data->delta_b = sqrt(delta_chi_2[confidence][DoF]) * sqrt(fit_data->sigma_2_b);

	llist_for_each_arg(data,(node_func_arg)find_chi_2,fit_data,&linear);

	fit_data->Q = gammaq((llist_size(data)-2) * .5,fit_data->chi_2 * 0.5);



}

/**
Fits data to a one parameter straight line model and provides uncertainty in that fit
*/
void one_param_straight_line_fit(FITDATA *fit_data,llist data,int confidence, int DoF){

	double S_xx,S_xy;
	double delta_chi_2[6][2] = {{1.00,2.30},
							   	{2.71,4.61},
							   	{4.00,6.17},
							   	{6.63,9.21},
							   	{9.00,11.8},
							   	{15.1,18.4}};
	FIT_TYPE linear = LINEAR;

	confidence -= 1;
	DoF -= 1;


	llist_for_each_arg(data,(node_func_arg)calculate_sums,fit_data,&linear);

	S_xx = fit_data->sums->S_xx;
	S_xy = fit_data->sums->S_xy;

	fit_data->delta = 0.0;
	fit_data->a = 0.0;
	fit_data->b = S_xy / S_xx;
	fit_data->sigma_2_a = 0.0;
	fit_data->sigma_2_b = 1/S_xx;
	fit_data->cov_ab = 0.0;
	fit_data->r_ab = 0.0;

	fit_data->delta_a = sqrt(delta_chi_2[confidence][DoF]) * sqrt(fit_data->sigma_2_a);
	fit_data->delta_b = sqrt(delta_chi_2[confidence][DoF]) * sqrt(fit_data->sigma_2_b);


	llist_for_each_arg(data,(node_func_arg)find_chi_2,fit_data,&linear);
	fit_data->Q = gammaq((llist_size(data)-1) * .5,fit_data->chi_2 * 0.5);


}

/**
Fits data to a quadratic model and provides uncertainty in that fit
*/
void quadratic_fit(FITDATA *fit_data,llist data,int confidence, int DoF){

	double S,S_x,S_y,S_xx,S_xy;
	double delta_chi_2[6][2] = {{1.00,2.30},
							   	{2.71,4.61},
							   	{4.00,6.17},
							   	{6.63,9.21},
							   	{9.00,11.8},
							   	{15.1,18.4}};
	FIT_TYPE quadratic = QUADRATIC;

	confidence -= 1;
	DoF -= 1;

	/*calculate_sums(fit_data,sigma,x_2,y,lag,pwr_level,length);*/
	llist_for_each_arg(data,(node_func_arg)calculate_sums,fit_data,&quadratic);

	S = fit_data->sums->S;
	S_x = fit_data->sums->S_x;
	S_y = fit_data->sums->S_y;
	S_xx = fit_data->sums->S_xx;
	S_xy = fit_data->sums->S_xy;

	fit_data->delta = S*S_xx - S_x*S_x;
	fit_data->a = (S_xx*S_y - S_x*S_xy)/fit_data->delta;
	fit_data->b = (S*S_xy - S_x*S_y)/fit_data->delta;
	fit_data->sigma_2_a = S_xx/fit_data->delta;
	fit_data->sigma_2_b = S/fit_data->delta;
	fit_data->cov_ab = -1 * S_x/fit_data->delta;
	fit_data->r_ab = -1 * S_x/sqrt(S * S_xx);

	fit_data->delta_a = sqrt(delta_chi_2[confidence][DoF]) * sqrt(fit_data->sigma_2_a);
	fit_data->delta_b = sqrt(delta_chi_2[confidence][DoF]) * sqrt(fit_data->sigma_2_b);



	llist_for_each_arg(data,(node_func_arg)find_chi_2,fit_data,&quadratic);
	fit_data->Q = gammaq((llist_size(data)-2) * .5,fit_data->chi_2 * 0.5);

}