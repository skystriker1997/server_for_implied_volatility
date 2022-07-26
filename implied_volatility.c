#include <stdio.h>
#include <stdlib.h>
#include <math.h>


void compute_d1(double*s, double*k, double*t, double*r, double*sigma, double*d1)  {
	*d1 = (log((*s)/(*k))+(*r+pow(*sigma, 2)/2)*(*t))/(*sigma)/sqrt(*t);
	return;
}

void compute_d2(double*s, double*k, double*t, double*r, double*sigma, double*d2)  {
	*d2 = (log((*s)/(*k))+(*r-pow(*sigma, 2)/2)*(*t))/(*sigma)/sqrt(*t);
	return;
}


void compute_nd1(double*d1, double*nd1)  {
	*nd1 = (1 + erf((*d1)/sqrt(2)))/2;
	return;
}

void compute_nd2(double*d2, double*nd2)  {
	*nd2 = (1 + erf((*d2)/sqrt(2)))/2;
	return;
}

double compute_vega(double*s, double*t, double*nd1)  {
	return (*s)*(*nd1)*sqrt(*t); 
} 


double compute_option_price(int* type, double*s, double*k, double*t, double*r, double*nd1, double*nd2)  {
	if (*type) 
		return (*s)*(*nd1)-(*k)*(*nd2)/exp((*r)*(*t));
	else
		return (*k)*(1-*nd2)/exp((*r)*(*t))-(*s)*(1-*nd1);
}



void compute_sigma(int* type, double*p, double*s, double*k, double*t, double*r, double*sigma, double*d1, double*d2, double*nd1, double*nd2)  {
	int i;
	double expected_option_price, expected_slope;
	*sigma = 0.05;
	for(i=0; i<1000; i++) {
		compute_d1(s, k, t, r, sigma, d1);
	    compute_d2(s, k, t, r, sigma, d2);
	    compute_nd1(d1, nd1);
	    compute_nd2(d2, nd2);
		expected_option_price = compute_option_price(type, s, k, t, r, nd1, nd2);
		expected_slope = compute_vega(s, t, nd1);
		*sigma = *sigma - (expected_option_price-*p)/expected_slope;
	}
	return;
}


int main(void)  {
	double s, k, t, r, sigma, p;
	int type;
	char *data;    
	double d1, d2, nd1, nd2;
	printf("%s%c%c\n", "Content-Type:text/html;charset=iso-8859-1",13,10);  
	printf("<TITLE>approximative result</TITLE>\n");
	data = getenv("QUERY_STRING");     
	if(data == NULL)  {
		printf("<P>Error! Error in passing data from form to script.");
		return 0;
	}
		
	if (sscanf(data,"type=%d&s=%lf&k=%lf&t=%lf&r=%lf&p=%lf",&type,&s,&k,&t,&r,&p)!=6)
		printf("<P>Error! Invalid data.");
	else  {
		compute_sigma(&type, &p, &s, &k, &t, &r, &sigma, &d1, &d2, &nd1, &nd2);
		printf("<H3>Approximative value of implied volatility is %lf.</H3>", sigma);
	}
	return 0;
}