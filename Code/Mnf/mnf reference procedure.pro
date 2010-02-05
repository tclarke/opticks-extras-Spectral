; This program seems to create noise ordered PCA or MNF and follows the routine by
; Morton Canty extracting the most important parts. The very unique feature of this
; version is that it extracts the region which has little spatial features automatically.
; This version also automatically suppresses bad bands which can easily occur if the atmospheric
; absorption bands are zeroed out.

function read_envi,filhdr,wav=wavdata
;+
; read ENVI file
;-
; read data
if strlen(filhdr) eq 0 then return,0
close,1
openr,1,filhdr
lin=''
datatype=4
while not(eof(1)) do begin
  readf,1,lin
  a=strsplit(lin,' ',/extract)
  case strlowcase(a(0)) of
  "samples": nx=long(a(2))
  "lines": ny=long(a(2))
  "bands": nz=long(a(2))
  "data": datatype=long(a(3))
  "interleave": interleave=a(2)
  "wavelength": begin
    if a(1) eq '=' then begin
    swav=''
    readf,1,swav
    while strpos(lin,'}') eq -1 do begin
      readf,1,lin
      swav=swav+lin
    endwhile
    swav=strmid(swav,0,strlen(swav)-1)
    wavdata=float(strsplit(swav,',',/extract))
    ;if datatype ne 12 then wavdata=wavdata/1000.
    endif
    end
  "bbl": begin
    sbbl=''
    readf,1,sbbl
    while strpos(lin,'}') eq -1 do begin
      readf,1,lin
      sbbl=sbbl+lin
    endwhile
    sbbl=strmid(sbbl,0,strlen(sbbl)-1)
    bbl=reform(long(strsplit(sbbl,',',/extract)))
    ibbl=where(bbl eq 1)
    end
  else:
  endcase
endwhile
close,1

; read data
if interleave eq 'bip' then begin
  filbip=strmid(filhdr,0,strpos(filhdr,'.hdr'))
  data=make_array(nz,nx,ny,type=datatype)
  ;if datatype eq 2 then data=intarr(nz,nx,ny) else data=fltarr(nz,nx,ny)
  openr,1,filbip
  readu,1,data
  close,1
  if datatype ne 1 then data=swap_endian(data)
endif

return,float(data)
end

pro gen_eigenproblem, C,B,A,lambda
;+
; NAME:
;       GEN_EIGENPROBLEM
; PURPOSE:
;       Solve the generalized eigenproblem
;                    C##a = lambda*B##a
;       using Cholesky factorization
; AUTHOR:
;       Mort Canty (2006)
;       Juelich Research Center
;       m.canty@fz-juelich.de
; CALLING SEQUENCE:
;       Gen_Eigenproblem, C, B, A, lambda
; ARGUMENTS:
;       C and B are real, square, symmetric matrices
;       B is positive definite
;       returns the eigenvalues in the row vector lambda
;       returns the eigenvectors a in the columns of A
; KEYWORDS:
;       None
; DEPENDENCIES
;       None
;-
; solve the generalized eigenproblem C##a = lambda*B##a
   choldc, B, P, /double
   for i=1,(size(B))[1]-1 do B[i,0:i]=[fltarr(i),P[i]]
   B[0,0]=P[0]
   Li  = invert(B,/double)
   D = Li ## C ## transpose(Li)
; ensure symmetry after roundoff errors
   D = (D+transpose(D))/2
   lambda = eigenql(D,/double,eigenvectors=A)
   A = transpose(A##Li)
end

PRO show_cube,data,win_id,tit,bsq=bsq,band=band
;+
; show a BIP image cube
;-
is=size(data)
if keyword_set(bsq) then begin
  nx=is(1)
  ny=is(2)
  nz=is(3)
  nxp=long(1200/nx)+1
  if nz lt 100 then nxp=8<nz
  if keyword_set(band) then begin
    window,win_id,xsize=nx,ysize=ny,title=tit
    tvscl,data(*,*,band)
  endif else begin
    window,win_id,xsize=nxp*nx,ysize=(nz/nxp)*ny,title=tit
    for i=0,nz-1 do tvscl,data(*,*,i),i
  endelse
endif else begin
  nz=is(1)
  nx=is(2)
  ny=is(3)
  nxp=long(1200/nx)+1
  if nz lt 100 then nxp=8<nz
  if keyword_set(band) then begin
    window,win_id,xsize=nx,ysize=ny,title=tit
    tvscl,reform(data(band,*,*))
  endif else begin
    window,win_id,xsize=nxp*nx,ysize=(nz/nxp)*ny,title=tit
    for i=0,nz-1 do tvscl,data(i,*,*),i
  endelse
endelse
return
end

function get_noise,data,varnoise,idiff=idiff,imeth=imeth,thres=thres,ibest=ibest,igood=igood
;+
; create an array with noise pixels
;-
; compute noise cube
is=size(data)
nz=is(1)
nx=long(is(2))
ny=is(3)

iok=where(varnoise gt 0.)
nok=n_elements(iok)

data_noise=fltarr(nok,nx,ny)

if keyword_set(imeth) eq 0 then imeth=0

; compute noise
case imeth of
0: begin
     ; compute the noise by difference of adjacent pixels
     for i=0,n_elements(iok)-1 do data_noise(i,0:nx-2,0:ny-2)=data(iok(i),0:nx-2,0:ny-2)-data(iok(i),1:*,1:*)
     data_noise(*,nx-1,*)=data_noise(*,0,*)
     data_noise(*,*,ny-1)=data_noise(*,*,0)
   end
1: begin
     ; high-pass filtering
     for i=0,n_elements(iok)-1 do data_noise(i,*,*)=data(iok(i),*,*)-mean(reform(data(iok(i),*,*)),5)
        end
else:
endcase

mask=intarr(nx,ny)

; speed calculation up by estimating mean and variance for only 1000 values
iarr=nx*ny*randomu(s,1000)
for iz=0,nok-1 do begin
  ; find the maximum variance of the data and determine a threshold of 1/10th
  x=reform(data_noise(iz,*,*),nx*ny)
  x=x-mean(x(iarr))
  sigma_max=stddev(x(iarr))/10.
  ; compute the pixel mask
  mask0=(abs(x) lt sigma_max)
  mask=mask+mask0
  if nok lt 10 then tvscl,mask
endfor
mask2=median(mask gt nok*thres,3)
window,6,title='Noise mask',xsize=nx,ysize=ny
tvscl,mask2

igood=where(mask2 eq 1)

if idiff eq 0 then return,(reform(data(iok,*,*),nok,nx*ny))(*,igood) $
else return,(reform(data_noise,nok,nx*ny))(*,igood)
end

PRO myMAF2
;+
; my implementation of MAF which is equivalent to MNF
;
;-
!p.color=0
!p.background=-1
!p.charsize=1.3
!order=1
;if ismall then begin
;  cd,'c:\important_files\Opticks_idl\'
;  data=float(read_envi('moffet64x64.hdr',wav=wav))
;  data0=data
;endif else begin
  cd,'PATH TO\SampleData'
  filhdr=dialog_pickfile(filter='*.hdr')
  data=read_envi(filhdr,wav=wav)
  if max(wav) gt 100. then wav=wav/1000.
  ired=where(abs(wav-0.63) eq min(abs(wav-0.63)))
  igreen=where(abs(wav-0.55) eq min(abs(wav-0.55)))
  iblue=where(abs(wav-0.48) eq min(abs(wav-0.48)))

  is=size(data)
  nxd=is(2)
  nyd=is(3)
  window,0,title='True color - select data',xsize=nxd,ysize=nyd
  tvscl,hist_equal(data([ired,igreen,iblue],*,*),percent=2),true=1,order=1
  ix0=nxd/2
  iy0=nyd/2
  nx0=nxd/4
  ny0=nyd/4
  box_cursor,ix0,iy0,nx0,ny0
  ix0=ix0>0
  ix1=(ix0+nx0-1)<(nxd-1)
  iy0=iy0>0
  iy0=(nyd-iy0-ny0)>0
  iy1=(iy0+ny0-1)<(nyd-1)
  data0=data(*,ix0:ix1,iy0:iy1)
  is=size(data0)
  nz=is(1)
  nx=is(2)
  ny=is(3)
  ismall=0
  if (nx lt 100) and (ny lt 100) then ismall=1
;endelse

ismall=0

; compute the variance in each band (bands have sometimes zero variance)
print,'computing variance'
varnoise=fltarr(nz)
iarr=nx*ny*randomu(s,1000)
for i=0,nz-1 do begin
  x=(reform(data0(i,*,*),nx*ny))(iarr)
  varnoise(i)=stddev(x)
endfor
print,'done with variance'

; the following two lines corrected a problem with the AVIRIS scene over Moffet Field
ibad=where(varnoise gt 2000.,count)
if count gt 0 then varnoise(ibad)=0.
iok=where(varnoise gt 0.)
nok=n_elements(iok)

swav=strtrim(iok(0),2)+'='+strtrim(wav(iok(0)),2)
for i=1,nok-1 do swav=swav+'|'+strtrim(iok(i),2)+'='+strtrim(wav(iok(i)),2)

ired=where(abs(wav-0.63) eq min(abs(wav-0.63)))
igreen=where(abs(wav-0.55) eq min(abs(wav-0.55)))
iblue=where(abs(wav-0.48) eq min(abs(wav-0.48)))

lp0:
desc=[$
'0,float,1.,label_left=Fraction of pixels for covariance calculation (<1)?,tag=frac',$
'0,droplist,'+swav+',label_left=Red,set_value='+strtrim(ired,2)+',tag=ired',$
'0,droplist,'+swav+',label_left=Green,set_value='+strtrim(igreen,2)+',tag=igreen',$
'0,droplist,'+swav+',label_left=Blue,set_value='+strtrim(iblue,2)+',tag=iblue',$
'0,button,Yes|No,label_left=Determine noise automatically,tag=inoise,/exclusive,set_value=1',$
'0,button,Row differencing|High-pass filter,label_left=Noise estimation method,tag=imeth,set_value=0,exclusive',$
'0,float,0.8,label_left=Threshold for automatic noise mask (<1.),tag=thres',$
'0,button,Original data|Noise difference data,label_left=Noise data option,tag=idiff,exclusive,set_value=0',$
'0,button,Largest region|All uniform pixels,label_left=Noise mask algorithm,exclusive,set_value=1,tag=ibest',$
'2,button,Run,quit']
in=cw_form(desc,title='Minimum noise fraction',/column)

ired=in.ired
igreen=in.igreen
iblue=in.iblue

is=size(data)
nz=is(1)
nx=long(is(2))
ny=is(3)
np=nx*ny


ismall=1
; estimate noise
if in.inoise eq 0 then begin
  print,'automatically find noise pixels'
  noise=get_noise(data,varnoise,imeth=in.imeth,idiff=in.idiff,$
    thres=in.thres,ibest=1-in.ibest,igood=igood)
  ians=dialog_message('Noise mask result Ok?',/question)
  if ians eq 'No' then goto,lp0
  data=data0
  is=size(data)
  nz=is(1)
  nx=is(2)
  ny=is(3)
endif else begin
  window,1,title='True color - select uniform dark area for noise',xsize=nxd,ysize=nyd
  tvscl,hist_equal(data(iok([ired,igreen,iblue]),*,*),percent=2),true=1,order=1
  is=size(data)
  nx=is(2)
  ny=is(3)
  ix0=nx/2
  iy0=ny/2
  nx0=nx/4
  ny0=ny/4
  box_cursor,ix0,iy0,nx0,ny0
  ix1=(ix0+nx0-1)<(nx-1)
  iy0=ny-iy0-ny0
  iy1=(iy0+ny0-1)<(ny-1)
  nnoise=(ix1-ix0+1)*(iy1-iy0+1)
  noise=reform(data(iok,ix0:ix1,iy0:iy1),nok,nnoise)
  data=data0
  is=size(data)
  nx=is(2)
  ny=is(3)
  if (nx lt 100) and (ny lt 100) then ismall=1
endelse

if ismall then show_cube,data,2,'Original image' else show_cube,data,2,'Original image',band=40
np=nx*ny
frac=in.frac<1.>0.
if nx*ny gt long(frac*np) then begin
  np=long(frac*np)
  ilist=long(nx*ny*randomu(s,np*1.3))
  ilist = ilist[UNIQ(ilist, SORT(ilist))]
  if n_elements(ilist) gt np  then ilist=ilist(0:np-1)
  np=n_elements(ilist)
  array=(reform(data(iok,*,*),nok,nx*ny))(*,ilist)
endif else array=reform(data(iok,*,*),nok,nx*ny)

; compute signal covariance matrix
; subtract mean
marray=total(double(array),2)/np
for i=0,nok-1 do array(i,*)=array(i,*)-replicate(marray(i),np)

print,'covariance of signal'
; compute covariance
sigma_signal=(transpose(array)##array)/(nx*ny-1)
fac=long(300/nok)+1
window,5,title='Covariance matrix: Signal | Noise',xsize=2*nok*fac,ysize=nok*fac
if fac eq 1 then tvscl,hist_equal(sigma_signal,percent=2),0 else $
  tvscl,rebin(hist_equal(sigma_signal,percent=2),nok*fac,nok*fac,/sample),0

nnoise=n_elements(noise)/nok
if nnoise eq 0 then begin ;gt long(frac*nx*ny) then begin
  np=long(frac*np)
  ilist=long(nnoise*randomu(s,np*1.3))
  ilist = ilist[UNIQ(ilist, SORT(ilist))]
  if n_elements(ilist) gt np then ilist=ilist(0:np-1)
  np=n_elements(ilist)
  noise=noise(*,ilist)
  print,'Using ', frac,' noise pixels'
endif else begin
  noise=noise
  np=nnoise
  print,'Using all noise pixels'
endelse

; subtract mean
mnoise=total(double(noise),2)/np
for i=0,nok-1 do noise(i,*)=noise(i,*)-replicate(mnoise(i),np)

; compute noise covariance
print,'compute covariance of noise'
sigma_noise=(transpose(noise)##noise)/(np-1)
if fac eq 1 then tvscl,hist_equal(sigma_noise,percent=2),0 else $
  tvscl,rebin(hist_equal(sigma_noise,percent=2),nok*fac,nok*fac,/sample),1
plots,fac*[nok,nok],[0,fac*nok],color=255,/device

print,'solve eigen problem'
; compute MAF eigen values
gen_eigenproblem, sigma_noise, sigma_signal, A, lambda

if min(lambda) lt 0. then begin
  print,'Negative eigenvalues - result is bogus!'
  stop
endif

window,3,title='SNR'
snr=2./reverse(lambda)
plot,snr,title= 'Signal to noise',xtitle=' Component number',ytitle='SNR',/ylog
ilimit=(where(snr le 10.))(0)
xyouts,0.2,0.85,strtrim(ilimit,2)+' components above SNR=10',/normal

; eigenvectors A are scaled so that MAFs have unit variance.
; ensure sum of positive correlations between X and MAF is positive
; their covariance matrix is cov_xm = Sigma##A
invsig_x = diag_matrix(1/sqrt(diag_matrix(Sigma_signal)))

; correlation matrix
corr_xm = invsig_x##Sigma_signal##A

; adjust sign of eigenvectors
sum = total(corr_xm,2)
A = A##diag_matrix(sum/abs(sum))

print,'compute MNF'
MAFs = reform(data(iok,*,*),nok,nx*ny) ## float(A)

MAFs=reform(MAFs,nok,nx,ny)
MAFs=MAFs(reverse(indgen(nok)),*,*)
if ismall then show_cube,mafs,4,'MAF'

window,7,xsize=nx,ysize=ny,title='Selection: False color MNF'
for i=0,2 do tvscl,hist_equal(mafs(i,*,*),perc=2),channel=i+1

; read data again
data=read_envi(filhdr,wav=wav)
; compute MNF's
data=data(iok,*,*)
is=size(data)
nz=is(1)
nx=long(is(2))
ny=is(3)
mnf_data=reform(reform(data,nok,nx*ny)##float(a(nok-3:nok-1,*)),3,nx,ny)
window,8,title='Entire image: MNF false color',xsize=nx,ysize=ny
for i=1,3 do tvscl,hist_equal(mnf_data(3-i,*,*),perc=2),channel=i

stop
end
