#' @importClassesFrom tidySummarizedExperiment SummarizedExperiment
#' @export
setClass("cytoExperiment", contains = c("tidySummarizedExperiment"))
         
#' @importFrom S4Vectors DataFrame SimpleList
#' @importFrom tidySummarizedExperiment SummarizedExperiment
#' @export
cytoExperiment <- function(cf){
  ca <- CytoArray(cf)
  pd <- pData(parameters(cf))
  
  ## somehow can't coerse df to DF
  # pd$name <- as.character(pd$name)
  # pd$desc <- as.character(pd$desc)
  # as(pd, "DataFrame")
  rd <- DataFrame(#name = pd$name
                   desc = pd$desc
                  , range = pd$range
                  , minRange = pd$minRange
                  , maxRange = pd$maxRange
                  , row.names = pd$name #rownames(pd)
                  )
  
  cse <- tidySummarizedExperiment(assays = SimpleList(intensity = ca)
                              , rowData = rd)
  as(cse, "cytoExperiment")
  
}
