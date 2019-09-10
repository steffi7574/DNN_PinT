#!/usr/bin/env python

# ----------------------------------------------------------------------
#  Imports
# ----------------------------------------------------------------------

import os, sys, shutil, copy
import numpy as np
from ordered_dict import OrderedDict
from ordered_bunch import OrderedBunch
from switch import switch

inf = 1.0e20


# ----------------------------------------------------------------------
#  Configuration Class
# ----------------------------------------------------------------------

class Config(OrderedBunch):
    """ config = Config(filename="")
        
        Starts a config class, an extension of 
        ordered_bunch()
        
        use 1: initialize by reading config file
            config = Config('filename')
        use 2: initialize from dictionary or bunch
            config = Config(param_dict)
        use 3: initialize empty
            config = Config()
        
        Parameters can be accessed by item or attribute
        ie: config['MESH_FILENAME'] or config.MESH_FILENAME
        
        Methods:
            read()       - read from a config file
            write()      - write to a config file (requires existing file)
            dump()       - dump a raw config file
            unpack_dvs() - unpack a design vector 
            diff()       - returns the difference from another config
            dist()       - computes the distance from another config
    """    

    _filename = 'config.cfg'
    
    def __init__(self,*args,**kwarg):
        
        # look for filename in inputs
        if args and isinstance(args[0],str):
            filename = args[0]
            args = args[1:]
        elif 'filename' in kwarg:
            filename = kwarg['filename']
            del kwarg['filename']
        else:
            filename = ''
        
        # initialize ordered bunch
        super(Config,self).__init__(*args,**kwarg)
        
        # read config if it exists
        if filename:
            try:
                self.read(filename)
            except IOError:
                print('Could not find config file: %s' % filename)
            except:
                print('Unexpected error: ', sys.exc_info()[0])
                raise
        
        self._filename = filename
    
    def read(self,filename):
        """ reads from a config file """
        konfig = read_config(filename)
        self.update(konfig)
        
    def write(self,filename=''):
        """ updates an existing config file """
        if not filename: filename = self._filename
        assert os.path.exists(filename) , 'must write over an existing config file'
        write_config(filename,self)
        
    def dump(self,filename=''):
        """ dumps all items in the config bunch, without comments """
        if not filename: filename = self._filename
        dump_config(filename,self)
    
    def __getattr__(self,k):
        try:
            return super(Config,self).__getattr__(k)
        except AttributeError:
            raise AttributeError('Config parameter not found')
        
    def __getitem__(self,k):
        try:
            return super(Config,self).__getitem__(k)
        except KeyError:
            raise KeyError('Config parameter not found: %s' % k)

       
    def __eq__(self,konfig):
        return super(Config,self).__eq__(konfig)
    def __ne__(self,konfig):
        return super(Config,self).__ne__(konfig)
    
    
    def __repr__(self):
        #return '<Config> %s' % self._filename
        return self.__str__()
    
    def __str__(self):
        output = 'Config: %s' % self._filename
        for k,v in self.items():
            output +=  '\n    %s= %s' % (k,v)
        return output
#: class Config







# -------------------------------------------------------------------
#  Get SU2 Configuration Parameters
# -------------------------------------------------------------------

def read_config(filename):
    """ reads a config file """
      
    # initialize output dictionary
    data_dict = OrderedDict()
    
    input_file = open(filename)
    
    # process each line
    while 1:
        # read the line
        line = input_file.readline()
        if not line:
            break
        
        # remove line returns
        line = line.strip('\r\n')
        # make sure it has useful data
        if (not "=" in line) or (line[0] == '#'):
            continue
        # split across equals sign
        line = line.split("=",1)
        this_param = line[0].strip()
        this_value = line[1].strip()
        
        assert this_param not in data_dict, ('Config file has multiple specifications of %s' % this_param )
        for case in switch(this_param):
            
            # Put all INTEGER options here
            if case("ntraining") or \
                case("nvalidation") or \
                case("nfeatures") or \
                case("nclasses") or \
                case("nchannels") or \
                case("nlayers") or \
                case("braid_cfactor") or \
                case("braid_cfactor0") or \
                case("braid_maxlevels") or \
                case("braid_mincoarse") or \
                case("braid_maxiter") or \
                case("braid_printlevel") or \
                case("braid_accesslevel") or \
                case("braid_setskip") or \
                case("braid_fmg") or \
                case("braid_nrelax") or \
                case("braid_nrelax0") or \
                case("ls_maxiter") or \
                case("lbfgs_stages") or \
                case("nbatch") or \
                case("validationlevel") :
                data_dict[this_param] = int(this_value)
                break

            # Put all FLOAT options here
            if case("T") or \
               case("braid_abstol") or \
               case("braid_adjtol") or \
               case("gamma_tik") or \
               case("gamma_ddt") or \
               case("gamma_class") or \
               case("stepsize") or \
               case("gtol") or \
               case("ls_factor") or \
               case("weights_open_init") or \
               case("weights_init") or \
               case("weights_class_init") :
               data_dict[this_param] = float(this_value)
               break

            # Put all STRING option here
            if case("activation") or \
               case("datafolder") or \
               case("ftrain_ex") or \
               case("ftrain_labels") or \
               case("fval_ex") or \
               case("fval_labels") or \
               case("weightsopenfile") or \
               case("weightsclassificationfile") or \
               case("network_type") or \
               case("type_openlayer") or \
               case("batch_type") or \
               case("stepsize_type") or \
               case("optim_maxiter") or \
               case("hessian_approx") :
               data_dict[this_param] = this_value
               break

    return data_dict
    
#: def read_config()


def dump_config(filename,config):
    ''' dumps a raw config file with all options in config 
        and no comments
    '''
           
    config_file = open(filename,'w')
    # write dummy file
    for key in config.keys():
        configline = str(key) + " = " + str(config[key]) + "\n"
        config_file.write(configline)
    config_file.close()
    # dump data
    #write_config(filename,config)    

