// Copyright
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Underlying paper:
//
// Layer-Parallel Training of Deep Residual Neural Networks
// S. Guenther, L. Ruthotto, J.B. Schroder, E.C. Czr, and N.R. Gauger
//
// Download: https://arxiv.org/pdf/1812.04352.pdf
//
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

#include "braid_wrapper.hpp"
#include "config.hpp"
#include "dataset.hpp"
#include "defs.hpp"
#include "hessianApprox.hpp"
#include "layer.hpp"
#include "network.hpp"
#include "util.hpp"

#define MASTER_NODE 0
#define FINITE_DIFFERENCE_TEST 1

int main(int argc, char *argv[]) {
  /* --- Data --- */
  Config *config;          /**< Storing configurations */
  DataSet *trainingdata;   /**< Training dataset */
  DataSet *validationdata; /**< Validation dataset */

  /* --- Network --- */
  Network *network; /**< DNN Network architecture */
  int startlayerID, endlayerID; /**< Index of first and last layer stored on this processor */
  MyReal accur_train = 0.0; /**< Accuracy on training data */
  MyReal accur_val = 0.0;   /**< Accuracy on validation data */
  MyReal loss_train = 0.0;  /**< Loss function on training data */
  MyReal loss_val = 0.0;    /**< Loss function on validation data */
  MyReal losstrain_out = 0.0;
  MyReal lossval_out = 0.0;
  MyReal accurtrain_out = 0.0;
  MyReal accurval_out = 0.0;

  /* --- XBraid --- */
  myBraidApp *primaltrainapp;         /**< Braid App for training data */
  myAdjointBraidApp *adjointtrainapp; /**< Adjoint Braid for training data */
  myBraidApp *primalvalapp;           /**< Braid App for validation data */

  /* --- Optimization --- */
  int ndesign_local;  /**< Number of local design variables on this processor */
  int ndesign_global; /**< Number of global design variables (sum of local)*/
  MyReal *ascentdir = 0; /**< Direction for design updates */
  MyReal objective;      /**< Optimization objective */
  MyReal wolfe;          /**< Holding the wolfe condition value */
  MyReal rnorm;          /**< Space-time Norm of the state variables */
  MyReal rnorm_adj;      /**< Space-time norm of the adjoint variables */
  MyReal gnorm;          /**< Norm of the gradient */
  MyReal ls_param;       /**< Parameter in wolfe condition test */
  MyReal stepsize;       /**< Stepsize used for design update */
  char optimfilename[255];
  FILE *optimfile = 0;
  MyReal ls_stepsize;
  MyReal ls_objective, test_obj;
  int ls_iter;

  /* --- Time measurements --- */
  struct rusage r_usage;
  MyReal StartTime, StopTime, myMB, globalMB;
  MyReal UsedTime = 0.0;

  /* Initialize MPI */
  int myid;
  int size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  /* Instantiate objects */
  config = new Config();
  trainingdata = new DataSet();
  validationdata = new DataSet();
  network = new Network(MPI_COMM_WORLD);

  /* Read config file */
  if (argc != 2) {
    if (myid == MASTER_NODE) {
      printf("\n");
      printf("USAGE: ./main </path/to/configfile> \n");
    }
    MPI_Finalize();
    return 0;
  }
  int err = config->readFromFile(argv[1]);
  if (err) {
    printf("Error while reading config file!\n");
    MPI_Finalize();
    return 0;
  }

  /* Initialize training and validation data */
  trainingdata->initialize(config->ntraining, config->nfeatures,
                           config->nclasses, config->nbatch, MPI_COMM_WORLD);
  trainingdata->readData(config->datafolder, config->ftrain_ex,
                         config->ftrain_labels);

  validationdata->initialize(config->nvalidation, config->nfeatures,
                             config->nclasses, config->nvalidation,
                             MPI_COMM_WORLD);  // full validation set!
  validationdata->readData(config->datafolder, config->fval_ex,
                           config->fval_labels);

  /* Initialize XBraid */
  primaltrainapp =
      new myBraidApp(trainingdata, network, config, MPI_COMM_WORLD);
  adjointtrainapp = new myAdjointBraidApp(
      trainingdata, network, config, primaltrainapp->getCore(), MPI_COMM_WORLD);
  primalvalapp =
      new myBraidApp(validationdata, network, config, MPI_COMM_WORLD);
  primaltrainapp->GetGridDistribution(&startlayerID, &endlayerID);
  if (startlayerID == 0) startlayerID = startlayerID - 1; // -1 is index of the opening layer

  /* Initialize the network  */
  network->createLayerBlock(startlayerID, endlayerID, config);
  network->setDesignRandom(config->weights_open_init, config->weights_init, config->weights_class_init);
  network->setDesignFromFile(config->datafolder, config->weightsopenfile, NULL, config->weightsclassificationfile);
  ndesign_local = network->getnDesignLocal();
  ndesign_global = network->getnDesignGlobal();

  /* Read design from file */
  // read_vector("design_optim_test.dat", network->getDesign(), ndesign_global);

  /* Print some neural network information */
  printf("%d: Layer range: [%d, %d] / %d\n", myid, startlayerID, endlayerID,
         config->nlayers);
  printf("%d: Design variables (local/global): %d/%d\n", myid, ndesign_local,
         ndesign_global);

  /* Initialize Hessian approximation */
  HessianApprox *hessian = NULL;
  switch (config->hessianapprox_type) {
    case BFGS_SERIAL:
      hessian = new BFGS(MPI_COMM_WORLD, ndesign_local);
      break;
    case LBFGS:
      hessian = new L_BFGS(MPI_COMM_WORLD, ndesign_local, config->lbfgs_stages);
      break;
    case IDENTITY:
      hessian = new Identity(MPI_COMM_WORLD, ndesign_local);
      break;
    default:
      printf("Error: unexpected hessianapprox_type returned");
      return 0;
  }

  /* Initialize optimization parameters */
  ascentdir = new MyReal[ndesign_local];
  stepsize = config->getStepsize(0);
  gnorm = 0.0;
  objective = 0.0;
  rnorm = 0.0;
  rnorm_adj = 0.0;
  ls_param = 1e-4;
  ls_iter = 0;
  ls_stepsize = stepsize;

  /* Open and prepare optimization output file*/
  if (myid == MASTER_NODE) {
    sprintf(optimfilename, "%s.dat", "optim");
    optimfile = fopen(optimfilename, "w");
    config->writeToFile(optimfile);
    fprintf(optimfile,
            "#    || r ||          || r_adj ||      Objective             Loss "
            "                 || grad ||            Stepsize  ls_iter   "
            "Accur_train  Accur_val   Time(sec)\n");
  }

  /* Measure wall time */
  StartTime = MPI_Wtime();
  StopTime = 0.0;
  UsedTime = 0.0;

  /** Main optimization iteration
   *
   * The following loop represents the paper's Algorithm (2)
   *
   */
  for (int iter = 0; iter < config->maxoptimiter; iter++) {
    /* Set up the current batch */
    trainingdata->selectBatch(config->batch_type, MPI_COMM_WORLD);

    /** Solve state and adjoint equations (2.15) and (2.17)
     *
     *  Algorithm (2): Step 1 and 2
     */
    printf("Run FWD Train:\n");
    rnorm = primaltrainapp->run();
    printf("Run BWD Train:\n");
    rnorm_adj = adjointtrainapp->run();

    /* Get output */
    objective = primaltrainapp->getObjective();
    loss_train = network->getLoss();
    accur_train = network->getAccuracy();

    printf("obj %1.14e loss %1.14e\n", objective, loss_train);

    /* --- Validation data: Get accuracy --- */
    if (config->validationlevel > 0) {
      printf("Run FWD Validation:\n");
      primalvalapp->run();
      loss_val = network->getLoss();
      accur_val = network->getAccuracy();
    }

    /* --- Optimization control and output ---*/

    /** Compute global gradient norm
     *
     *  Algorithm (2): Step 3
     */
    gnorm = vecnorm_par(ndesign_local, network->getGradient(), MPI_COMM_WORLD);

    /* Communicate loss and accuracy. This is actually only needed for output.
     * TODO: Remove it. */
    MPI_Allreduce(&loss_train, &losstrain_out, 1, MPI_MyReal, MPI_SUM,
                  MPI_COMM_WORLD);
    MPI_Allreduce(&loss_val, &lossval_out, 1, MPI_MyReal, MPI_SUM,
                  MPI_COMM_WORLD);
    MPI_Allreduce(&accur_train, &accurtrain_out, 1, MPI_MyReal, MPI_SUM,
                  MPI_COMM_WORLD);
    MPI_Allreduce(&accur_val, &accurval_out, 1, MPI_MyReal, MPI_SUM,
                  MPI_COMM_WORLD);

    /* Output */
    StopTime = MPI_Wtime();
    UsedTime = StopTime - StartTime;
    if (myid == MASTER_NODE) {
      printf(
          "\niter \t|| r ||\t\t|| r_adj ||\tObjective\t\tLoss\t\t\t|| grad "
          "||\t\tStepsize\tls_iter\tAccur_train\tAccur_val\tTime(sec)\n");
      printf(
          "%03d\t%1.8e\t%1.8e\t%1.14e\t%1.14e\t%1.14e\t%5f\t%2d\t%2.2f%%\t\t%2."
          "2f%%\t\t%.1f\n\n",
          iter, rnorm, rnorm_adj, objective, losstrain_out, gnorm, stepsize,
          ls_iter, accurtrain_out, accurval_out, UsedTime);
      fprintf(optimfile,
              "%03d  %1.8e  %1.8e  %1.14e  %1.14e  %1.14e  %5f  %2d        "
              "%2.2f%%      %2.2f%%     %.1f\n",
              iter, rnorm, rnorm_adj, objective, losstrain_out, gnorm, stepsize,
              ls_iter, accurtrain_out, accurval_out, UsedTime);
      fflush(optimfile);
    }

    /** Check optimization convergence
     *
     *  Algorithm (2): Step 6
     */
    if (gnorm < config->gtol) {
      if (myid == MASTER_NODE) {
        printf("Optimization has converged. \n");
        printf("Be happy and go home!       \n");
      }
      break;
    }
    if (iter == config->maxoptimiter - 1) {
      if (myid == MASTER_NODE) {
        printf("\nMax. optimization iterations reached.\n");
      }
      break;
    }

    /* If optimization didn't converge, continue */

    /* --- Design update --- */

    /** Compute search direction
     *
     *  Algorithm (2): Step 4
     */
    hessian->updateMemory(iter, network->getDesign(), network->getGradient());
    hessian->computeAscentDir(iter, network->getGradient(), ascentdir);
    stepsize = config->getStepsize(iter);

    /** Update the design/network control parameter in negative ascent direction
     *  and perform backtracking linesearch.
     *
     *  Algorithm (2): Step 5
     */
    vec_axpy(ndesign_local, -1.0*stepsize, ascentdir, network->getDesign());
    network->MPI_CommunicateNeighbours();

    if (config->stepsize_type == BACKTRACKINGLS) {
      /* Compute wolfe condition */
      wolfe = vecdot_par(ndesign_local, network->getGradient(), ascentdir,
                         MPI_COMM_WORLD);

      /* Start linesearch iterations */
      ls_stepsize = config->getStepsize(iter);
      stepsize = ls_stepsize;
      for (ls_iter = 0; ls_iter < config->ls_maxiter; ls_iter++) {
        primaltrainapp->getCore()->SetPrintLevel(0);
        primaltrainapp->run();
        ls_objective = primaltrainapp->getObjective();
        primaltrainapp->getCore()->SetPrintLevel(config->braid_printlevel);

        test_obj = objective - ls_param * ls_stepsize * wolfe;
        // if (myid == MASTER_NODE)
          // printf("ls_iter = %d:\tls_objective = %1.14e\ttest_obj = %1.14e\n",
                //  ls_iter, ls_objective, test_obj);
        /* Test the wolfe condition */
        if (ls_objective <= test_obj) {
          /* Success, use this new design */
          break;
        } else {
          /* Test for line-search failure */
          if (ls_iter == config->ls_maxiter - 1) {
            if (myid == MASTER_NODE)
              printf("\n\n   WARNING: LINESEARCH FAILED! \n\n");
            break;
          }

          /* Go back part of the step */
          vec_axpy(ndesign_local, (1.0 - config->ls_factor) * stepsize, ascentdir, network->getDesign());
          network->MPI_CommunicateNeighbours();

          /* Decrease the stepsize */
          ls_stepsize = ls_stepsize * config->ls_factor;
          stepsize = ls_stepsize;
        }
      }
    }
  }

  /* --- Run final validation and write prediction file --- */
  if (config->validationlevel > -1) {
    if (myid == MASTER_NODE) printf("\n --- Run final validation ---\n");

    // primalvalapp->getCore()->SetPrintLevel(0);
    primalvalapp->run();
    loss_val = network->getLoss();

    printf("Final validation loss:  %1.14e\n", loss_val);
  }

  write_vector("design_optim.dat", network->getDesign(), ndesign_global);
  write_vector("gradient.dat", network->getGradient(), ndesign_global);

  /* Print some statistics */
  StopTime = MPI_Wtime();
  UsedTime = StopTime - StartTime;
  getrusage(RUSAGE_SELF, &r_usage);
  myMB = (MyReal)r_usage.ru_maxrss / 1024.0;
  MPI_Allreduce(&myMB, &globalMB, 1, MPI_MyReal, MPI_SUM, MPI_COMM_WORLD);

  // printf("%d; Memory Usage: %.2f MB\n",myid, myMB);
  if (myid == MASTER_NODE) {
    printf("\n");
    printf(" Used Time:        %.2f seconds\n", UsedTime);
    printf(" Global Memory:    %.2f MB\n", globalMB);
    printf(" Processors used:  %d\n", size);
    printf("\n");
  }


#if FINITE_DIFFERENCE_TEST
  /* #######################################################*/
  /* Finite Difference Gradient Test (central, 2nd order) */
  /* #######################################################*/

  double EPS = 1e-5;

  /* Compute gradient */
  primaltrainapp->run();
  double obj_orig = primaltrainapp->getObjective();
  printf("obj_orig = %1.14e\n", obj_orig);
  adjointtrainapp->run();

  /* Loop over design vector */
  printf("i   Gradient             FD                   error\n");
  for (int idesign = 0; idesign < ndesign_local; idesign++) {
    /* Store original design */
    double design_orig = network->getDesign()[idesign];

    /* Perturb forward: design + EPS */
    network->getDesign()[idesign] = design_orig + EPS;
    primaltrainapp->run();
    double obj_perturb1 = primaltrainapp->getObjective();

    /* Perturb backward: design - EPS */
    network->getDesign()[idesign] = design_orig - EPS;
    primaltrainapp->run();
    double obj_perturb2 = primaltrainapp->getObjective();

    /* Compute FD and error */
    double FD_grad = (obj_perturb1 - obj_perturb2) / (2*EPS);
    // double FD_grad = (obj_perturb1 - obj_orig) / (EPS);
    double error = network->getGradient()[idesign] - FD_grad;
    double err_rel = -1.0;
    if (FD_grad != 0.0) err_rel = error / FD_grad;

    printf("%d: %1.14e  %1.14e   %1.5e\n", idesign, network->getGradient()[idesign], FD_grad, err_rel);

    /* Restore design */
    network->getDesign()[idesign] = design_orig;
  }

#endif

  /* Clean up XBraid */
  delete network;

  delete primaltrainapp;
  delete adjointtrainapp;
  delete primalvalapp;

  /* Delete optimization vars */
  delete hessian;
  delete[] ascentdir;

  /* Delete training and validation examples  */
  delete trainingdata;
  delete validationdata;

  /* Close optim file */
  if (myid == MASTER_NODE) {
    fclose(optimfile);
    printf("Optimfile: %s\n", optimfilename);
  }

  delete config;

  MPI_Finalize();
  return 0;
}
