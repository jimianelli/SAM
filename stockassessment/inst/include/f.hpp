template <class Type>
Type trans(Type x){
  return Type(2)/(Type(1) + exp(-Type(2) * x)) - Type(1);
}

template <class Type>
Type nllF(confSet &conf, paraSet<Type> &par, array<Type> &logF, data_indicator<vector<Type>,Type> &keep, objective_function<Type> *of){
  using CppAD::abs;
  Type nll=0; 
  int stateDimF=logF.dim[0];
  int timeSteps=logF.dim[1];
  int noFleets=conf.keyLogFsta.dim[0];
  int stateDimN=conf.keyLogFsta.dim[1];
  vector<Type> sdLogFsta=exp(par.logSdLogFsta);
  array<Type> resF(stateDimF,timeSteps-1);
  matrix<Type> fvar(stateDimF,stateDimF);
  matrix<Type> fcor(stateDimF,stateDimF);
  vector<Type> fsd(stateDimF);  

  fcor.setZero();
  for(int i=0; i<stateDimF; ++i){
    fcor(i,i)=1.0;
  }
  
  
  int count=0; //corFlags may miss come fleets, need to correct for that
  for(int f=0;f<noFleets;f++){
    for(int i=0; i<stateDimF; ++i){
      for(int j=0; j<i; ++j){
        if(conf.keyLogFsta(f,j)>(-1)){
          if(conf.corFlag(f)==1){
            fcor(i,j)=trans(par.itrans_rho(f-count));
            fcor(j,i)=fcor(i,j);
            ++count;
          }  
        }   
      }
    }

    for(int i=0; i<stateDimF; ++i){
      for(int j=0; j<i; ++j){
        if(conf.keyLogFsta(f,j)>(-1)){
          if(conf.corFlag(f)==2){
            fcor(i,j)=pow(trans(par.itrans_rho(f-count)),abs(Type(i-j)));
            fcor(j,i)=fcor(i,j);
            ++count;
          }
        }  
      }
    }

    int i,j;
    for(i=0; i<stateDimF; ++i){
	  for(j=0; j<stateDimN; j++){
	    if(conf.keyLogFsta(f,j)>(-1)){
          if(conf.keyLogFsta(f,j)==i)break;
          fsd(i)=sdLogFsta(conf.keyVarF(f,j));        
        }
      }
    }
  }
 
  for(int i=0; i<stateDimF; ++i){
    for(int j=0; j<stateDimF; ++j){
      fvar(i,j)=fsd(i)*fsd(j)*fcor(i,j);
    }
  }
  density::MVNORM_t<Type> neg_log_densityF(fvar);
  Eigen::LLT< Matrix<Type, Eigen::Dynamic, Eigen::Dynamic> > lltCovF(fvar);
  matrix<Type> LF = lltCovF.matrixL();
  matrix<Type> LinvF = LF.inverse();

  for(int i=1;i<timeSteps;i++){
    resF.col(i-1) = LinvF*(vector<Type>(logF.col(i)-logF.col(i-1)));    
    nll+=neg_log_densityF(logF.col(i)-logF.col(i-1)); // F-Process likelihood
    SIMULATE_F(of){
      if(conf.simFlag==0){
        logF.col(i)=logF.col(i-1)+neg_log_densityF.simulate();
      }
    }
  }

  if(CppAD::Variable(keep.sum())){ // add wide prior for first state, but _only_ when computing ooa residuals
    Type huge = 10;
    for (int i = 0; i < stateDimF; i++) nll -= dnorm(logF(i, 0), Type(0), huge, true);  
  } 

  if(conf.resFlag==1){
    ADREPORT_F(resF,of);
  }
  return nll;
}
