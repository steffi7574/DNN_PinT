#include "braid_wrapper.hpp"
// #include "codi.hpp"
// #include "lib.hpp"


int 
my_Step(braid_App        app,
        braid_Vector     ustop,
        braid_Vector     fstop,
        braid_Vector     u,
        braid_StepStatus status)
{
    // int    ts;
    // double tstart, tstop;
    // double *bias, *weights;
    // double deltaT;
    // int    nchannels = app->layer->nchannels;

    // int nelem;
    // if (app->training)
    // {
    //     nelem = app->ntraining;
    // }
    // else
    // {
    //     nelem = app->nvalidation;
    // }
    
    // /* Get the time-step size */
    // braid_StepStatusGetTstartTstop(status, &tstart, &tstop);
    // deltaT = tstop - tstart;
 
    // /* Get the current time index */
    // braid_StepStatusGetTIndex(status, &ts);
 
    // /* Set the layer parameters */
    // bias      = &(app->theta[ts*(nchannels * nchannels+1) + nchannels*nchannels]);
    // weights   = &(app->theta[ts*(nchannels * nchannels+1)]);

    // app->layer->setBias(bias);
    // app->layer->setWeights(weights);
    // app->layer->setDt(deltaT);

    // /* apply the layer for all examples */
    // for (int ielem = 0; ielem < nelem; ielem++)
    // {
    //     double* data = &(u->Y[ielem * nchannels]);
    //     app->layer->applyFWD(data);
    // }

    // /* no refinement */
    // braid_StepStatusSetRFactor(status, 1);
 
    return 0;
}   


int
my_Init(braid_App     app,
        double        t,
        braid_Vector *u_ptr)
{
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;


    /* Allocate the vector */
    my_Vector* u = (my_Vector *) malloc(sizeof(my_Vector));
    u->state = new double*[nexamples];
    for (int iex = 0; iex < nexamples; iex < nexamples)
    {
        u->state[iex] = new double[nchannels];
    }

    /* Project data to the network layer at t=0.0 */
    if (t == 0.0)
    {
        /* apply the layer for all examples */
        for (int iex = 0; iex < nexamples; iex++)
        {
            app->network->openlayer->applyFWD(app->examples[iex], u->state[iex]);
        }
    }
    else
    {
        /* Initialize with zero everywhere else */
        for (int iex = 0; iex < nexamples; iex++)
        {
            for (int ic = 0; ic < nchannels; ic++)
            {
                u->state[iex][ic] = 0.0;
            }
        }
    }

    *u_ptr = u;

    return 0;
}


int
my_Init_diff(braid_App     app,
             double        t,
             braid_Vector  ubar)
{
    int nexamples = app->nexamples;

    if (t == 0)
    {
        /* apply the opening layer backwards for all examples */
        for (int iex = 0; iex < nexamples; iex++)
        {
            // pass auxilliary state_curr as placeholder for recomputing the state,
            // pass NULL because data_in_bar would be derivative of the input data, which is not computed. 
            app->network->openlayer->applyBWD(app->examples[iex], app->network->state_curr, NULL, ubar->state[iex]);
        }
    }

    return 0;
}         


int
my_Clone(braid_App     app,
         braid_Vector  u,
         braid_Vector *v_ptr)
{
    // my_Vector *v;
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;
 
    /* Allocate the vector */
    my_Vector* v = (my_Vector *) malloc(sizeof(my_Vector));
    v->state = new double*[nexamples];
    for (int iex = 0; iex < nexamples; iex < nexamples)
    {
        v->state[iex] = new double[nchannels];
    }

    /* Clone the values */
    for (int i = 0; i < nexamples * nchannels; i++)
    {
        v->state[i] = u->state[i];
    }

    *v_ptr = v;

    return 0;
}


int
my_Free(braid_App    app,
        braid_Vector u)
{
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;

    for (int iex = 0; iex < nexamples; iex < nexamples)
    {
        delete [] u->state[iex];
    }
    delete u->state;

   return 0;
}


int
my_Sum(braid_App     app,
       double        alpha,
       braid_Vector  x,
       double        beta,
       braid_Vector  y)
{
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;

    for (int iex = 0; iex < nexamples; iex < nexamples)
    {
        for (int ic = 0; ic < nchannels; ic++)
        {
           (y->state)[iex][ic] = alpha*(x->state)[iex][ic] + beta*(y->state)[iex][ic];
        }
    }

   return 0;
}

int
my_SpatialNorm(braid_App     app,
               braid_Vector  u,
               double       *norm_ptr)
{
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;

    double dot = 0.0;
    for (int iex = 0; iex < nexamples; iex++)
    {
        dot += vecdot(nchannels, u->state[iex], u->state[iex]);
    }
   *norm_ptr = sqrt(dot) / nexamples;

   return 0;
}



int
my_Access(braid_App          app,
          braid_Vector       u,
          braid_AccessStatus astatus)
{
    printf("my_Access: To be implemented...\n");

    return 0;
}


int
my_BufSize(braid_App           app,
           int                 *size_ptr,
           braid_BufferStatus  bstatus)
{
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;
   
    *size_ptr = nchannels*nexamples*sizeof(double);
    return 0;
}



int
my_BufPack(braid_App           app,
           braid_Vector        u,
           void               *buffer,
           braid_BufferStatus  bstatus)
{
    double *dbuffer   = (double*) buffer;
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;
    
    for (int iex = 0; iex < nexamples; iex < nexamples)
    {
        for (int ic = 0; ic < nchannels; ic++)
        {
           dbuffer[iex * nchannels + ic] = (u->state)[iex][ic];
        }
    }

    braid_BufferStatusSetSize( bstatus,  nchannels*nexamples*sizeof(double));
 
   return 0;
}



int
my_BufUnpack(braid_App           app,
             void               *buffer,
             braid_Vector       *u_ptr,
             braid_BufferStatus  bstatus)
{

    double *dbuffer   = (double*) buffer;
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;
    
    //  /* Allocate the vector */
    my_Vector* u = (my_Vector *) malloc(sizeof(my_Vector));
    u->state = new double*[nexamples];
    for (int iex = 0; iex < nexamples; iex++)
    {
        u->state[iex] = new double[nchannels];
    }

    /* Unpack the buffer */
    for (int iex = 0; iex < nexamples; iex++)
    {
        for (int ic = 0; ic < nchannels; ic++)
        {
           u->state[iex][ic] = dbuffer[iex * nchannels + ic];
        }
    }
 
    *u_ptr = u;
    return 0;
}


int 
my_ObjectiveT(braid_App              app,
              braid_Vector           u,
              braid_ObjectiveStatus  ostatus,
              double                *objective_ptr)
{
    int class_id = -1;
    int success  = 0;
    double loss       = 0.0;
    double objective  = 0.0;
    double regul_tikh = 0.0;
    double regul_ddt  = 0.0;

    int nlayers   = app->network->getnLayers();
    int nchannels = app->network->getnChannels();
    int nexamples = app->nexamples;

    /* Get the time index*/
    int ts;
    braid_ObjectiveStatusGetTIndex(ostatus, &ts);
 
    if (ts == 0) // Only tikhonov on first layer 
    {
        regul_tikh += app->network->openlayer->evalTikh();
    }
    else if (ts < nlayers) // tikhonov and ddt regularization on intermediate layers 
    {
        regul_tikh += app->network->layers[ts-1]->evalTikh();
        if (ts > 1) regul_ddt += app->network->evalRegulDDT(app->network->layers[ts-2], app->network->layers[ts-1]);
    }
    else // tikhonov regularization and loss evaluation on last layer
    {
        /* Tikhonov regularization */
        regul_tikh += app->network->endlayer->evalTikh();

        for (int iex = 0; iex < nexamples; iex++)
        {
            /* Apply classification layer */
            app->network->endlayer->applyFWD(u->state[iex], app->network->state_final);

            /* Evaluate loss */
            loss += app->network->endlayer->evalLoss(app->network->state_final, app->labels[iex]);
        
            /* Test for successful prediction */
            class_id = app->network->endlayer->prediction(app->network->state_final);
            if ( app->labels[iex][class_id] > 0.99 )  
            {
                success++;
            }
        }

        /* Compute objective function */
        loss       = 1. / nexamples * loss;
        // objective  = loss + gamma_tik * regul_tikh + gamma_ddt * regul_ddt;
    
        /* Compute network accuracy */
        // app->accuracy = 100.0 * (double) success / nexamples;
    }


    //    /* Add regularization for classifier */
    //    regul  = tikhonov_regul(app->classW, nclasses * nchannels);
    //    regul += tikhonov_regul(app->classMu, nclasses);

    //    app->class_regul = app->gamma_class * regul;
    //    obj             += app->gamma_class * regul;

    //    /* Compute accuracy */
    //    app->accuracy = 100.0 * (double) success / nelem;  
    // }

    *objective_ptr = objective;
    
    
    return 0;
}


int
my_ObjectiveT_diff(braid_App            app,
                  braid_Vector          u,
                  braid_Vector          u_bar,
                  braid_Real            f_bar,
                  braid_ObjectiveStatus ostatus)
{
    // int ntraining = app->ntraining;
    // int nchannels = app->nchannels;
    // int nlayers    = app->nlayers;
    // int nclasses  = app->nclasses;
    // int nfeatures = app->nfeatures;
    // int ntheta_open = app->ntheta_open;
    // int ntheta    = (nchannels * nchannels + 1 ) * nlayers;
    // int nstate    = nchannels * ntraining;
    // int nclassW   = nchannels*nclasses;
    // int nclassmu  = nclasses;
    // int ts, itheta, success;
    // RealReverse regul;
    // RealReverse obj = 0.0;

    // if (!app->training)
    // {
    //     printf("\nERROR: Do not compute gradient on validation error!\n\n");
    //     exit(1);
    // }
 
    // /* Get the time index*/
    // braid_ObjectiveStatusGetTIndex(ostatus, &ts); 

    // /* Set up CoDiPack */
    // RealReverse::TapeType& codiTape = RealReverse::getGlobalTape();
    // codiTape.setActive();

    // /* Register input */
    // RealReverse* Ycodi;        /* CodiType for the state */
    // RealReverse* theta;        /* CodiType for theta */
    // RealReverse* theta_open;   /* CodiType for theta at opening layer */
    // RealReverse* classW;       /* CoDiTypye for classW */
    // RealReverse* classMu;      /* CoDiTypye for classMu */
    // Ycodi      = (RealReverse*) malloc(nstate      * sizeof(RealReverse));
    // theta      = (RealReverse*) malloc(ntheta      * sizeof(RealReverse));
    // theta_open = (RealReverse*) malloc(ntheta_open * sizeof(RealReverse));
    // classW     = (RealReverse*) malloc(nclassW     * sizeof(RealReverse));
    // classMu    = (RealReverse*) malloc(nclassmu    * sizeof(RealReverse));
    // for (int i = 0; i < nstate; i++)
    // {
    //     Ycodi[i] = u->Y[i];
    //     codiTape.registerInput(Ycodi[i]);
    // }
    // for (int i = 0; i < ntheta; i++)
    // {
    //     theta[i] = app->theta[i];
    //     codiTape.registerInput(theta[i]);
    // }
    // for (int i = 0; i < ntheta_open; i++)
    // {
    //     theta_open[i] = app->theta_open[i];
    //     codiTape.registerInput(theta_open[i]);
    // }
    // for (int i = 0; i < nclassW; i++)
    // {
    //     classW[i] = app->classW[i];
    //     codiTape.registerInput(classW[i]);
    // }
    // for (int i=0; i<nclassmu; i++)
    // {
    //     classMu[i] = app->classMu[i];
    //     codiTape.registerInput(classMu[i]);
    // }
    

    // /* Tape the objective function evaluation */
    // if (ts == 0)
    // {
    //     /* Compute regularization term for opening layer */
    //     obj = app->gamma_theta_tik * tikhonov_regul(theta_open, ntheta_open);
    // }
    // else 
    // {
    //     /* Compute regularization term */
    //     itheta = (ts - 1 ) * (nchannels * nchannels + 1 ) ;
    //     obj  = app->gamma_theta_tik * tikhonov_regul(&(theta[itheta]), nchannels * nchannels +1);
    //     obj += app->gamma_theta_ddt * ddt_theta_regul(theta, ts, app->deltaT, nlayers, nchannels);
    // }

    // /* At last layer: Evaluate Loss and add classification regularization */
    // if (ts == nlayers)
    // {
    //     /* Evaluate loss at last layer*/
    //    obj = 1./app->ntraining * loss(Ycodi, app->Ctrain, app->Ytrain, classW, classMu, ntraining, nclasses, nchannels, nfeatures, app->output, &success);

    //    /* Add regularization for classifier */
    //    regul  = tikhonov_regul(classW, nclasses * nchannels);
    //    regul += tikhonov_regul(classMu, nclasses);
    //    obj   += app->gamma_class * regul;
    // } 
    
    // /* Set the seed */
    // codiTape.setPassive();
    // obj.setGradient(f_bar);

    // /* Evaluate the tape */
    // codiTape.evaluate();

    // /* Update adjoint variables and gradient */
    // for (int i = 0; i < nstate; i++)
    // {
    //     u_bar->Y[i] = Ycodi[i].getGradient();
    // }
    // for (int i = 0; i < ntheta; i++)
    // {
    //     app->theta_grad[i] += theta[i].getGradient();
    // }
    // for (int i = 0; i < ntheta_open; i++)
    // {
    //     app->theta_open_grad[i] += theta_open[i].getGradient();
    // }
    // for (int i = 0; i < nclassW; i++)
    // {
    //     app->classW_grad[i] += classW[i].getGradient();
    // }
    // for (int i=0; i < nclassmu; i++)
    // {
    //     app->classMu_grad[i] += classMu[i].getGradient();
    // }

    // /* Reset the codi tape */
    // codiTape.reset();

    // /* Clean up */
    // free(Ycodi);
    // free(theta);
    // free(theta_open);
    // free(classW);
    // free(classMu);

   return 0;
}

int
my_Step_diff(braid_App         app,
             braid_Vector      ustop,     /**< input, u vector at *tstop* */
             braid_Vector      u,         /**< input, u vector at *tstart* */
             braid_Vector      ustop_bar, /**< input / output, adjoint vector for ustop */
             braid_Vector      u_bar,     /**< input / output, adjoint vector for u */
             braid_StepStatus  status)
{

    // double  tstop, tstart, deltaT;
    // double *bias, *bias_bar, *weights, *weights_bar;
    // double *data, *data_bar;
    // int     ts;
    // int     nchannels   =   app->layer->nchannels;

    // if (!app->training)
    // {
    //     printf("\nERROR: Do not compute gradient on validation error!\n\n");
    //     exit(1);
    // }
 
    // /* Get time and time step */
    // braid_StepStatusGetTstartTstop(status, &tstart, &tstop);
    // braid_StepStatusGetTIndex(status, &ts);
    // deltaT = tstop - tstart;


    // /* Set the layer parameters */
    // bias        = &(app->theta[ts*(nchannels * nchannels+1) + nchannels*nchannels]);
    // bias_bar    = &(app->theta_grad[ts*(nchannels * nchannels+1) + nchannels*nchannels]);
    // weights     = &(app->theta[ts*(nchannels * nchannels+1)]);
    // weights_bar = &(app->theta_grad[ts*(nchannels * nchannels+1)]);


    // app->layer->setBias(bias);
    // app->layer->setBias_bar(bias_bar);
    // app->layer->setWeights(weights);
    // app->layer->setWeights_bar(weights_bar);
    // app->layer->setDt(deltaT);


    // /* apply the layer backwards for all examples */
    // for (int ielem = 0; ielem < app->ntraining; ielem++)
    // {
    //     data     = &(u->Y[ielem * nchannels]);
    //     data_bar = &(u_bar->Y[ielem * nchannels]);
    //     app->layer->applyBWD(data, data_bar);
    // }

    return 0;
}

int 
my_ResetGradient(braid_App app)
{
    // int ntheta_open = app->nfeatures  * app->nchannels + 1;
    // int ntheta      = (app->nchannels * app->nchannels + 1) * app->nlayers;
    // int nclassW     =  app->nchannels * app->nclasses;
    // int nclassmu    =  app->nclasses;

    // /* Set the gradient to zero */
    // for (int i = 0; i < ntheta_open; i++)
    // {
    //     app->theta_open_grad[i] = 0.0;
    // }
    // for (int i = 0; i < ntheta; i++)
    // {
    //     app->theta_grad[i] = 0.0;
    // }
    // for (int i = 0; i < nclassW; i++)
    // {
    //     app->classW_grad[i] = 0.0;
    // }
    // for (int i = 0; i < nclassmu; i++)
    // {
    //     app->classMu_grad[i] = 0.0;
    // }


    return 0;
}
