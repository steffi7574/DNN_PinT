#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "config.hpp"
#include "dataset.hpp"
#include "layer.hpp"
#include "util.hpp"
#pragma once

/* The network class logically connects the layers. Each processor owns one 
 * block of the global network containing a portion of all layers with indices in
 * [startlayerID, endLayerID], where startlayerID >= -1 (-1 for the openinglayer), endlayerID <= nlayers_global -2) (nlayers_global -2 is the classification layer).  
 * Pointers to these layers are stored in the vector **layers.  
 * Most important routines are createLayerBlock() which creates the layers
 * and allocates the weights, and setInitialDesign() which provides an 
 * for the network weights. 
*/
class Network {
 protected:
  int nlayers_global; /* Total number of Layers of the network */
  int nlayers_local;  /* Number of Layers in this network block */

  int nchannels;   /* Width of the network */
  MyReal dt;       /* Time step size */
  MyReal loss;     /* Value of the loss function */
  MyReal accuracy; /* Accuracy of the network prediction (percentage of
                      successfully predicted classes) */

  int startlayerID; /* ID of the first layer on that processor */
  int endlayerID;   /* ID of the last layer on that processor */

  int ndesign_global;   /* Global number of design vars  */
  int ndesign_local;    /* Number of design vars of this local network block  */
  int ndesign_layermax; /* Max. number of design variables of all hidden layers
                         */

  MyReal *design;   /* Local vector of design variables*/
  MyReal *gradient; /* Local Gradient */

  Layer **layers;    /* Array of layers */
  Layer *layer_left; /* Copy of last layer of left-neighbouring processor */
  Layer *layer_right;/* Copy of first layer of right-neighbouring processor */

  MPI_Comm comm; /* MPI communicator */
  int mpirank;   /* rank of this processor */

 public:
  Network(MPI_Comm comm);

  ~Network();

  /* This processor creates a network block containing layers at all time steps 
   * in the interval [startLayerID, endLayerID]. 
   * Here, the design and gradient vectors containing the weights, biases and 
   * their gradients for those layers is allocated. 
   */
  void createLayerBlock(int StartLayerID, int EndLayerID, Config *config, int current_nhiddenlayers);

  /* Get number of channels */
  int getnChannels();

  /* Get global number of layers */
  int getnLayersGlobal();

  /* Get initial time step size */
  MyReal getDT();

  /* Get local storage index of the a layer */
  int getLocalID(int ilayer);

  /* Return value of the loss function */
  MyReal getLoss();

  /* Return accuracy value */
  MyReal getAccuracy();

  /* Return a pointer to the design vector */
  MyReal *getDesign();

  /* Return a pointer to the gradient vector */
  MyReal *getGradient();

  /* Get ID of first and last layer on this processor */
  int getStartLayerID();
  int getEndLayerID();

  /**
   *  Return number of design variables (local on this processor or global) */
  int getnDesignLocal();
  int getnDesignGlobal();

  /* Return ndesign_layermax */
  int getnDesignLayermax();

  /* Return MPI communicator */
  MPI_Comm getComm();

  /**
   * Get the layer at a certain layer index, i.e. a certain time step
   * Returns NULL, if this layer is not stored on this processor
   */
  Layer *getLayer(int layerindex);

  /*
   * Sets the design vector of all layers to random values, scaled by the given factors
   */
  void setDesignRandom(MyReal factor_open, MyReal factor_hidden, MyReal factor_classification);


  /*
   * Interpolate a design from a coarser network to this one.
   * Coarse and fine-grid network layers MUST have same dimensions!
   * NI_interp_type: 0  : Carries out piece-wise constant everywhere
   *                 1  : Carries out linear interpolation everywhere, except at
   *                      the last interval of new layers where piece-wise
   *                      constant is used.
   */
  void interpolateDesign(int rfactor, Network* coarse_net, int NI_interp_type);


  /* 
   * Reads in design variables from file
   * Currently only opening weights and classification weights can be read. 
   */
  void setDesignFromFile(const char* datafolder, const char* openingfilename, const char* hiddenfilename, const char* classificationfilename);



  /*
   * Return a newly constructed layer. The time-step index decides if it is
   * an openinglayer (-1), a hidden layer, or a classification layer
   * (nlayers_global-2). The config file provides information on the kind of 
   * layer that is to be created (Dense or Convolutional). 
   */
  Layer *createLayer(int index, Config *config);

  /* Replace the layer with one that is received from the left neighbouring
   * processor */
  int MPI_CommunicateNeighbours();

  /**
   * Applies the classification and evaluates loss/accuracy
   */
  void evalClassification(DataSet *data, MyReal **state, int output);

  /**
   * On classification layer: derivative of evalClassification
   */
  void evalClassification_diff(DataSet *data, MyReal **primalstate,
                               MyReal **adjointstate, int compute_gradient);

};
