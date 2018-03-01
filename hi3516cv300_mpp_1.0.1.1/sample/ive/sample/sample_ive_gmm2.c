#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>


#include "hi_common.h"
#include "hi_comm_video.h"
#include "hi_comm_sys.h"
#include "hi_comm_ive.h"

#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_ive.h"

#include "sample_comm_ive.h"

#define SAMPLE_IVE_GMM2_MAX(a,b) ((a) > (b) ? (a) : (b))

typedef struct hiSAMPLE_IVE_GMM2_S
{
    IVE_SRC_IMAGE_S astSrc[2];
	IVE_DST_IMAGE_S stFg;
	IVE_DST_IMAGE_S stBg;
	IVE_SRC_IMAGE_S	stFactor;
	IVE_DST_IMAGE_S	stMatchModelInfo;
	IVE_SRC_IMAGE_S stFgMask;
	IVE_SRC_IMAGE_S stLastImg;
	IVE_DST_IMAGE_S stDiffImg;
	IVE_DST_IMAGE_S stMagImg;
	IVE_DST_IMAGE_S stCurNormMag;
	IVE_DST_IMAGE_S stLastNormMag;	
	IVE_DST_MEM_INFO_S stModel;
	IVE_DST_MEM_INFO_S stBlob;
	IVE_GMM2_CTRL_S stGmm2Ctrl;

	FILE *pFpSrc;
	FILE *pFpFg;
	FILE *pFpBg;
} SAMPLE_IVE_GMM2_S;

static SAMPLE_IVE_GMM2_S 	s_stGmm2;
static HI_VOID SAMPLE_IVE_Gmm2_Uninit(SAMPLE_IVE_GMM2_S *pstGmm2)
{
	HI_U16 i;
	for (i = 0; i < 2; i++)
	{
		IVE_MMZ_FREE(pstGmm2->astSrc[i].u32PhyAddr[0], pstGmm2->astSrc[i].pu8VirAddr[0]);
	}
    IVE_MMZ_FREE(pstGmm2->stFg.u32PhyAddr[0], pstGmm2->stFg.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stBg.u32PhyAddr[0], pstGmm2->stBg.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stFactor.u32PhyAddr[0], pstGmm2->stFactor.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stMatchModelInfo.u32PhyAddr[0], pstGmm2->stMatchModelInfo.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stFgMask.u32PhyAddr[0], pstGmm2->stFgMask.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stDiffImg.u32PhyAddr[0], pstGmm2->stDiffImg.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stMagImg.u32PhyAddr[0], pstGmm2->stMagImg.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stCurNormMag.u32PhyAddr[0], pstGmm2->stCurNormMag.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stLastNormMag.u32PhyAddr[0], pstGmm2->stLastNormMag.pu8VirAddr[0]);
    IVE_MMZ_FREE(pstGmm2->stModel.u32PhyAddr, pstGmm2->stModel.pu8VirAddr);
    IVE_MMZ_FREE(pstGmm2->stBlob.u32PhyAddr, pstGmm2->stBlob.pu8VirAddr);

	IVE_CLOSE_FILE(pstGmm2->pFpSrc);
	IVE_CLOSE_FILE(pstGmm2->pFpFg);	
	IVE_CLOSE_FILE(pstGmm2->pFpBg);
}

static HI_S32 SAMPLE_IVE_Gmm2_Init(SAMPLE_IVE_GMM2_S *pstGmm2,HI_U16 u16Width,HI_U16 u16Height, HI_CHAR *pchSrcFileName,
			HI_CHAR *pchFgFileName,HI_CHAR *pchBgFileName)
{
    HI_S32 s32Ret = HI_SUCCESS;
	HI_U32 u32Size;
	HI_U16 i,j;
	HI_U8 *pu8Tmp;
    memset(pstGmm2, 0, sizeof(SAMPLE_IVE_GMM2_S));

	for (i = 0; i < 2; i++)
	{
		s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->astSrc[i]), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
		if (s32Ret != HI_SUCCESS)
		{
			SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
			goto GMM2_INIT_FAIL;
		}

	}
	
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stFg), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }

    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stBg), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stFactor), IVE_IMAGE_TYPE_U16C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }
	pu8Tmp = pstGmm2->stFactor.pu8VirAddr[0];
	for (i = 0; i < pstGmm2->stFactor.u16Height; i++)
	{
		for (j = 0; j < pstGmm2->stFactor.u16Width; j++)
		{
			pu8Tmp[2 * j ]    	= 8;
			pu8Tmp[ 2 * j + 1] 	= 4;
		}
		pu8Tmp += pstGmm2->stFactor.u16Stride[0] * sizeof(HI_U16);
	}
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stMatchModelInfo), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stFgMask), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }
 
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stDiffImg), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }

    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stMagImg), IVE_IMAGE_TYPE_U16C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stCurNormMag), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }
    s32Ret = SAMPLE_COMM_IVE_CreateImage(&(pstGmm2->stLastNormMag), IVE_IMAGE_TYPE_U8C1, u16Width, u16Height);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("SAMPLE_COMM_IVE_CreateImage fail\n");
        goto GMM2_INIT_FAIL;
    }

   pstGmm2->stGmm2Ctrl.u16VarRate			 = 1;
   pstGmm2->stGmm2Ctrl.u8ModelNum			 = 3;
   pstGmm2->stGmm2Ctrl.u9q7MaxVar			 = (16 * 16)<<7;
   pstGmm2->stGmm2Ctrl.u9q7MinVar			 = ( 8 *  8)<<7;
   pstGmm2->stGmm2Ctrl.u8GlbSnsFactor		 = 8;
   pstGmm2->stGmm2Ctrl.enSnsFactorMode		 = IVE_GMM2_SNS_FACTOR_MODE_PIX;
   pstGmm2->stGmm2Ctrl.u16FreqThr			 = 12000;
   pstGmm2->stGmm2Ctrl.u16FreqInitVal		 = 20000;
   pstGmm2->stGmm2Ctrl.u16FreqAddFactor		 = 0xEF;
   pstGmm2->stGmm2Ctrl.u16FreqReduFactor 	 = 0xFF00;
   pstGmm2->stGmm2Ctrl.u16LifeThr			 = 5000;
   pstGmm2->stGmm2Ctrl.enLifeUpdateFactorMode = IVE_GMM2_LIFE_UPDATE_FACTOR_MODE_GLB;

   u32Size = pstGmm2->stGmm2Ctrl.u8ModelNum * 8 * pstGmm2->astSrc[0].u16Width * pstGmm2->astSrc[0].u16Height;
   s32Ret = SAMPLE_COMM_IVE_CreateMemInfo(&pstGmm2->stModel, u32Size);
   if (s32Ret != HI_SUCCESS)
   {
	   SAMPLE_PRT("SAMPLE_COMM_IVE_CreateMemInfo fail\n");
	   goto GMM2_INIT_FAIL;
   }
   memset(pstGmm2->stModel.pu8VirAddr, 0, pstGmm2->stModel.u32Size);
   u32Size = sizeof(IVE_CCBLOB_S);	
   s32Ret = SAMPLE_COMM_IVE_CreateMemInfo(&pstGmm2->stBlob, u32Size);
   if (s32Ret != HI_SUCCESS)
   {
		SAMPLE_PRT("SAMPLE_COMM_IVE_CreateMemInfo fail\n");
		goto GMM2_INIT_FAIL;
   }

	pstGmm2->pFpSrc = fopen(pchSrcFileName, "rb");  
	if (NULL == pstGmm2->pFpSrc)
	{
	    SAMPLE_PRT("Open file %s fail\n", pchSrcFileName);
	    s32Ret = HI_FAILURE;
	    goto GMM2_INIT_FAIL;
	}
	pstGmm2->pFpFg = fopen(pchFgFileName, "wb");  
	if (NULL == pstGmm2->pFpFg)
	{
	    SAMPLE_PRT("Open file %s fail\n", pchFgFileName);
	    s32Ret = HI_FAILURE;
	    goto GMM2_INIT_FAIL;
	}
	pstGmm2->pFpBg = fopen(pchBgFileName, "wb");  
	if (NULL == pstGmm2->pFpBg)
	{
	    SAMPLE_PRT("Open file %s fail\n", pchBgFileName);
	    s32Ret = HI_FAILURE;
	    goto GMM2_INIT_FAIL;
	}
  
GMM2_INIT_FAIL:
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_IVE_Gmm2_Uninit(pstGmm2);
    }
    return s32Ret;
}

static HI_S32 SAMPLE_IVE_GenFgMask(IVE_SRC_IMAGE_S *pstFg,IVE_DST_IMAGE_S *pstFgMask)
{
	HI_S32 s32Ret;	
	IVE_HANDLE IveHandle;
	IVE_THRESH_CTRL_S stCtrl;
	
	stCtrl.enMode	= IVE_THRESH_MODE_BINARY;
	stCtrl.u8MinVal = 0;
	stCtrl.u8MaxVal = 1;
	stCtrl.u8LowThr = 5;
	s32Ret = HI_MPI_IVE_Thresh(&IveHandle, pstFg, pstFgMask, &stCtrl, HI_FALSE);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("HI_MPI_IVE_Thresh fail,Error(%#x)\n",s32Ret);
		return s32Ret;
	}

	return s32Ret;
}
/*
*Reduce the factor gradually to the default value
*/
static HI_VOID SAMPLE_IVE_ReduFactor(IVE_IMAGE_S *pstFactor)
{
	HI_U16 i,j;
	HI_U8 *pu8Tmp;
	
	pu8Tmp = pstFactor->pu8VirAddr[0];
	for (i = 0; i < pstFactor->u16Height; i++)
	{		
		for (j = 0; j < pstFactor->u16Width; j++)
		{
			pu8Tmp[j<<1]		= SAMPLE_IVE_GMM2_MAX(8, pu8Tmp[i<<1] - 1);
			pu8Tmp[(i<<1) + 1]	= SAMPLE_IVE_GMM2_MAX(8, pu8Tmp[(i<<1) + 1] - 2);
		}
		pu8Tmp += pstFactor->u16Stride[0] * sizeof(HI_U16);
	}
}
/*
*Change factor by difference frame
*/
static HI_BOOL SAMPLE_IVE_ChgFactorByDiffFrm(IVE_SRC_IMAGE_S *pstSrc,IVE_SRC_IMAGE_S *pstLastImg,
														IVE_DST_IMAGE_S *pstDiffImg,IVE_DST_IMAGE_S *pstFactor)
{
	HI_S32 s32Ret;
	IVE_HANDLE IveHandle;
	HI_BOOL bFinish;
	HI_BOOL bBlock = HI_TRUE;
	HI_U16 i,j;	
	HI_S32 s32AbsTh = 10;
	HI_U32 u32PointSum = 0;
	IVE_SUB_CTRL_S stSubCtrl;
	HI_U8 *pu8Tmp,*pu8Factor;
	
	stSubCtrl.enMode = IVE_SUB_MODE_ABS;
	s32Ret = HI_MPI_IVE_Sub(&IveHandle, pstSrc, pstLastImg, pstDiffImg, &stSubCtrl, HI_TRUE);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("HI_MPI_IVE_Sub fail,Error(%#x)\n",s32Ret);
		return HI_FALSE;
	}

	//Wait task finish
    s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
    while (HI_ERR_IVE_QUERY_TIMEOUT == s32Ret)
    {
        usleep(100);
        s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
    }
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_IVE_Query fail,Error(%#x)\n", s32Ret);
        return HI_FALSE;
    }

	pu8Tmp = pstDiffImg->pu8VirAddr[0];
	for (i = 0; i < pstSrc->u16Height; i++)
	{
		for (j = 0; j < pstSrc->u16Width; j++)
		{
			u32PointSum += pu8Tmp[j] > s32AbsTh;
		}
		pu8Tmp += pstDiffImg->u16Stride[0];
	}
	

	if ((u32PointSum * 3) > (pstSrc->u16Width * pstSrc->u16Height* 2))
	{
		pu8Tmp   = pstDiffImg->pu8VirAddr[0];
		pu8Factor = pstFactor->pu8VirAddr[0];
		for (i = 0; i < pstSrc->u16Height; i++)
		{
			for (j = 0; j < pstSrc->u16Width; j++)
			{
				if (pu8Tmp[j] > s32AbsTh)
				{
					pu8Factor[j<<1] 	 	= 20;
					pu8Factor[(j<<1) + 1] 	= 200;

				}
			}
			pu8Tmp 		+= pstDiffImg->u16Stride[0];
			pu8Factor 	+= pstFactor->u16Stride[0] * sizeof(HI_U16);
		}

		return HI_TRUE;		

	}

	return HI_FALSE;

}

/*
*Change factor by gradient
*/
static HI_VOID SAMPLE_IVE_ChgFactorByGrad(IVE_SRC_IMAGE_S *pstFgMask,
				IVE_SRC_IMAGE_S *pstSrc,IVE_SRC_IMAGE_S *pstLastImg,IVE_SRC_IMAGE_S *pstLastNormMag,
				IVE_DST_IMAGE_S *pstMagImg,IVE_DST_IMAGE_S *pstCurNormMag,IVE_DST_MEM_INFO_S *pstBlob,
				IVE_DST_IMAGE_S *pstDiffImg,IVE_DST_IMAGE_S *pstFactor)
{
	HI_S32 s32Ret;
	IVE_HANDLE IveHandle;
	HI_BOOL bFinish;
	HI_BOOL bBlock = HI_TRUE;
	HI_U16 i,j,k;
	HI_U16 u16Top;
	HI_U16 u16Left;
	HI_U16 u16Right;
	HI_U16 u16Bottom;
	HI_U8 *pu8FgCur;
	HI_U8 *pu8GradDiff;
	HI_U8 *pu8Factor;
	HI_S32 s32FgSum    = 0;
	HI_S32 s32GradSum  = 0;			

	HI_S8 au8Mask[25] = {0, 0,0,0,0, 
						0,-1,0,1,0, 
						0,-2,0,2,0, 
						0,-1,0,1,0, 
						0, 0,0,0,0};
	IVE_CCL_CTRL_S stCclCtrl;	
	IVE_CCBLOB_S   *pstCclBlob;	
	IVE_MAG_AND_ANG_CTRL_S   stMagAndAngCtrl;	
	IVE_16BIT_TO_8BIT_CTRL_S st16To8Ctrl;	
	IVE_SUB_CTRL_S stSubCtrl;

	stCclCtrl.enMode   = IVE_CCL_MODE_8C;
	stCclCtrl.u16Step  = 2;
	stCclCtrl.u16InitAreaThr = 4;
	s32Ret = HI_MPI_IVE_CCL(&IveHandle, pstFgMask, pstBlob, &stCclCtrl, HI_TRUE);	
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("HI_MPI_IVE_Sub fail,Error(%#x)\n",s32Ret);
		return;
	}
	//Wait task finish
    s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
    while (HI_ERR_IVE_QUERY_TIMEOUT == s32Ret)
    {
        usleep(100);
        s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
    }
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_IVE_Query fail,Error(%#x)\n", s32Ret);
        return;
    }

	pstCclBlob = (IVE_CCBLOB_S *)pstBlob->pu8VirAddr;
	if (pstCclBlob->u8RegionNum > 0)
	{
		//Calc the gradient difference of the current image and the last image		
		memcpy(stMagAndAngCtrl.as8Mask, au8Mask, 25);
		stMagAndAngCtrl.u16Thr		 = 0;
		stMagAndAngCtrl.enOutCtrl	 = IVE_MAG_AND_ANG_OUT_CTRL_MAG;
		s32Ret = HI_MPI_IVE_MagAndAng(&IveHandle, pstSrc, pstMagImg, HI_NULL, &stMagAndAngCtrl, HI_FALSE);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("HI_MPI_IVE_MagAndAng fail,Error(%#x)\n",s32Ret);
			return ;
		}
		st16To8Ctrl.s8Bias		 	= 0;
		st16To8Ctrl.enMode		 	= IVE_16BIT_TO_8BIT_MODE_U16_TO_U8;
		st16To8Ctrl.u8Numerator	 	= 255;
		st16To8Ctrl.u16Denominator 	= 255 * 4;
		s32Ret = HI_MPI_IVE_16BitTo8Bit(&IveHandle, pstMagImg, pstCurNormMag, &st16To8Ctrl, HI_FALSE);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("HI_MPI_IVE_16BitTo8Bit fail,Error(%#x)\n",s32Ret);
			return;
		}
		s32Ret = HI_MPI_IVE_MagAndAng(&IveHandle, pstLastImg, pstMagImg, HI_NULL, &stMagAndAngCtrl, HI_FALSE);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("HI_MPI_IVE_MagAndAng fail,Error(%#x)\n",s32Ret);
			return;
		}
		s32Ret = HI_MPI_IVE_16BitTo8Bit(&IveHandle, pstMagImg, pstLastNormMag, &st16To8Ctrl, HI_FALSE);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("HI_MPI_IVE_16BitTo8Bit fail,Error(%#x)\n",s32Ret);
			return;
		}
		stSubCtrl.enMode = IVE_SUB_MODE_ABS;
		s32Ret = HI_MPI_IVE_Sub(&IveHandle, pstCurNormMag, pstLastNormMag, pstDiffImg, &stSubCtrl, HI_TRUE);
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("HI_MPI_IVE_Sub fail,Error(%#x)\n",s32Ret);
			return;
		}
		s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
		while (HI_ERR_IVE_QUERY_TIMEOUT == s32Ret)
		{
			usleep(100);
			s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
		}
		if (HI_SUCCESS != s32Ret)
		{
			SAMPLE_PRT("HI_MPI_IVE_Query fail,Error(%#x)\n", s32Ret);
			return;
		}

		
	}

	//for each blob, analyze the gradient change
	for(k = 0; k < IVE_MAX_REGION_NUM; k++)
	{
		if(0 == pstCclBlob->astRegion[k].u32Area) 
		{
			continue;
		}

		u16Top    =  pstCclBlob->astRegion[k].u16Top;
		u16Left   =  pstCclBlob->astRegion[k].u16Left;
		u16Right  =  pstCclBlob->astRegion[k].u16Right;
		u16Bottom =  pstCclBlob->astRegion[k].u16Bottom;
		if((u16Right - u16Left) * (u16Bottom - u16Top) < 60)
		{
			continue;
		}

		s32FgSum    = 0;
		s32GradSum  = 0;
		pu8FgCur    = pstFgMask->pu8VirAddr[0] + u16Top * pstFgMask->u16Stride[0];
		pu8GradDiff = pstDiffImg->pu8VirAddr[0] + u16Top * pstDiffImg->u16Stride[0];
		for (i = u16Top; i < u16Bottom; i++)
		{
			for (j = u16Left; j < u16Right; j++)
			{
				if (pu8FgCur[j])
				{
					s32FgSum++;
					if (pu8GradDiff[j])
					{
						s32GradSum++;
					}
				}
			}

			pu8FgCur    += pstFgMask->u16Stride[0];
			pu8GradDiff += pstDiffImg->u16Stride[0];
		}

		if ((s32GradSum < 10) || (s32GradSum * 100 < s32FgSum))
		{
			pu8Factor = pstFactor->pu8VirAddr[0] + 2 * u16Top * pstFactor->u16Stride[0];
			pu8FgCur = pstFgMask->pu8VirAddr[0] + u16Top * pstFgMask->u16Stride[0];
			for (i = u16Top; i < u16Bottom; i++)
			{
				for (j = u16Left; j < u16Right; j++)
				{
					if (pu8FgCur[j])
					{
						pu8Factor[j<<1] = 16;
						pu8Factor[(j<<1) + 1] = 200;
					}
				}
				pu8FgCur     += pstFgMask->u16Stride[0];
				pu8Factor    += pstFactor->u16Stride[0] * sizeof(HI_U16);
			}
		}
	}

}
/*
*Adjustment factor
*/
static HI_VOID SAMPLE_IVE_AdjustmentFactor(IVE_SRC_IMAGE_S *pstFactor, 
							 IVE_SRC_IMAGE_S *pstFgMask, IVE_SRC_IMAGE_S *pstSrc,
							 IVE_SRC_IMAGE_S *pstLastImg, IVE_DST_IMAGE_S *pstDiffImg,
							 IVE_DST_IMAGE_S *pstMagImg, IVE_DST_IMAGE_S *pstCurNormMag,
							 IVE_DST_IMAGE_S *pstLastNormMag, IVE_DST_MEM_INFO_S *pstBlob)
{

	HI_BOOL bChg;
	//First, reduce the factor gradually to the default value------------------------------
	SAMPLE_IVE_ReduFactor(pstFactor);

	//Second, analyze the frame difference-------------------------------------------------
	//When the number of changed pixels is more than the threshold, there maybe a light switch.
	//And then, we should set a big factor to adapt to it quickly.
	bChg = SAMPLE_IVE_ChgFactorByDiffFrm(pstSrc,pstLastImg,pstDiffImg,pstFactor);
	if (HI_TRUE == bChg)
	{
		return;
	}
	
	//Third, analyze the gradient for foreground blobs------------------------------------
	//When gradient change of a foreground blob is very small, it maybe a local illumination change,
	//a ghost, or a static object. 
	//Here we try to reduce the influence by a local illumination change or a ghost only.
	SAMPLE_IVE_ChgFactorByGrad(pstFgMask,pstSrc,pstLastImg,pstLastNormMag,
								pstMagImg,pstCurNormMag,pstBlob,
								pstDiffImg,	pstFactor);
}

HI_VOID SAMPLE_IVE_Gmm2Proc(SAMPLE_IVE_GMM2_S *pstGmm2)
{
	HI_S32 s32Ret;	
    IVE_HANDLE IveHandle;
    HI_BOOL bFinish = HI_FALSE;
    HI_BOOL bBlock = HI_TRUE;
    HI_BOOL bInstant = HI_TRUE;
	HI_U32 u32FrmNum;
	HI_S32 s32CurIdx = 0;
	HI_U32 u32TotalFrm = 700;
	
	for (u32FrmNum = 1;u32FrmNum < u32TotalFrm;u32FrmNum++)
	{
		
		SAMPLE_PRT("Proc Frame %d/%d\n",u32FrmNum,u32TotalFrm);
		s32Ret = SAMPLE_COMM_IVE_ReadFile(&(pstGmm2->astSrc[s32CurIdx]), pstGmm2->pFpSrc);
	    if (s32Ret != HI_SUCCESS)
	    {
	        SAMPLE_PRT("SAMPLE_COMM_IVE_ReadFile fail\n");
	        return;
	    }

		//To building a stable background model quickly at the begin, some parameters are set specially.
		if (1 == pstGmm2->stGmm2Ctrl.u8ModelNum)
		{
			//If the parameter u8ModelNum is set to 1, the parameter u16FreqReduFactor 
			//is usually set to a small value at the first N frames. Here, N = 500.
			pstGmm2->stGmm2Ctrl.u16FreqReduFactor = (u32FrmNum >= 500) ? 0xFFA0 : 0xFC00;
		}
		else
		{
			//If the parameter u8ModelNum is more than 1, the global life mode should be used at the first N frames,
			//and the parameter u16GlbLifeUpdateFactor is usually set to a big value. Here, N = 500.
			if(u32FrmNum >= 500)
			{
				pstGmm2->stGmm2Ctrl.enLifeUpdateFactorMode = IVE_GMM2_LIFE_UPDATE_FACTOR_MODE_PIX;
			}
			else
			{
				pstGmm2->stGmm2Ctrl.u16GlbLifeUpdateFactor = 0xFFFF / u32FrmNum;
			}
		}

		//GMM2
		s32Ret = HI_MPI_IVE_GMM2(&IveHandle,&pstGmm2->astSrc[s32CurIdx],&pstGmm2->stFactor,&pstGmm2->stFg,&pstGmm2->stBg,
			&pstGmm2->stMatchModelInfo,&pstGmm2->stModel,&pstGmm2->stGmm2Ctrl,bInstant);
		if(HI_SUCCESS != s32Ret)
		{			
	        SAMPLE_PRT("HI_MPI_IVE_GMM2 fail,Error(%#x)\n",s32Ret);
			return;
		}

		//factor adjustment
		if (u32FrmNum > 1)
		{
			s32Ret = SAMPLE_IVE_GenFgMask(&(pstGmm2->stFg),&(pstGmm2->stFgMask));
			if(HI_SUCCESS != s32Ret)
			{			
				SAMPLE_PRT("SAMPLE_IVE_GenFgMask fail,Error(%#x)\n",s32Ret);
				return;
			}
			SAMPLE_IVE_AdjustmentFactor(&(pstGmm2->stFactor), &(pstGmm2->stFgMask), 
									&(pstGmm2->astSrc[s32CurIdx]),&(pstGmm2->astSrc[1 - s32CurIdx]),
									&(pstGmm2->stDiffImg), &(pstGmm2->stMagImg), &(pstGmm2->stCurNormMag),
									&(pstGmm2->stLastNormMag), &(pstGmm2->stBlob));
		
		}
		else
		{
			s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
			while (HI_ERR_IVE_QUERY_TIMEOUT == s32Ret)
			{
				usleep(100);
				s32Ret = HI_MPI_IVE_Query(IveHandle, &bFinish, bBlock);
			}
			if (HI_SUCCESS != s32Ret)
			{
				SAMPLE_PRT("HI_MPI_IVE_Query fail,Error(%#x)\n", s32Ret);
				return ;
			}
		}
	
		s32Ret = SAMPLE_COMM_IVE_WriteFile(&(pstGmm2->stFg), pstGmm2->pFpFg);
	    if (s32Ret != HI_SUCCESS)
	    {
	        SAMPLE_PRT("SAMPLE_COMM_IVE_WriteFile fail\n");
	        return;
	    }
		
		s32Ret = SAMPLE_COMM_IVE_WriteFile(&(pstGmm2->stBg), pstGmm2->pFpBg);
	    if (s32Ret != HI_SUCCESS)
	    {
	        SAMPLE_PRT("SAMPLE_COMM_IVE_WriteFile fail\n");
	        return;
	    }
		
		//change cur image to last image
		s32CurIdx = 1 - s32CurIdx;
	}
	
}

HI_VOID SAMPLE_IVE_Gmm2(HI_VOID)
{
    HI_S32 				s32Ret;
    HI_U16 				u16Width  = 352;
    HI_U16 				u16Height = 288;
    HI_CHAR				*pchSrcFileName = "./data/input/gmm2/gmm2_352x288_sp400_frm1000.yuv";
    HI_CHAR				*pchFgFileName = "./data/output/gmm2/fg_352x288_sp400.yuv";
    HI_CHAR				*pchBgFileName = "./data/output/gmm2/bg_352x288_sp400.yuv";

	memset(&s_stGmm2,0,sizeof(s_stGmm2));
    SAMPLE_COMM_IVE_CheckIveMpiInit();

   	s32Ret = SAMPLE_IVE_Gmm2_Init(&s_stGmm2, u16Width,u16Height,pchSrcFileName,
						pchFgFileName,pchBgFileName);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_IVE_Gmm2_Init fail\n");
        goto GMM2_FAIL;
    }

	SAMPLE_IVE_Gmm2Proc(&s_stGmm2);

    SAMPLE_IVE_Gmm2_Uninit(&s_stGmm2);
	memset(&s_stGmm2,0,sizeof(s_stGmm2));

GMM2_FAIL:
    SAMPLE_COMM_IVE_IveMpiExit();
}
/******************************************************************************
* function : Gmm2 sample signal handle
******************************************************************************/
HI_VOID SAMPLE_IVE_Gmm2_HandleSig(HI_VOID)
{
    SAMPLE_IVE_Gmm2_Uninit(&s_stGmm2);
	memset(&s_stGmm2,0,sizeof(s_stGmm2));
    SAMPLE_COMM_IVE_IveMpiExit();
}
