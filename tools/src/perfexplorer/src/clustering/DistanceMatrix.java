/*
 * Created on Mar 16, 2005
 *
 */
package edu.uoregon.tau.perfexplorer.clustering;


 /**
  * This class is used to build a square distance matrix.  The distance matrix
  * is used to hold the distances between elements in a vector.  The matrix
  * is a diagonal matrix, and only the lower-left quadrants below the
  * diagonal are used.
  *
  * <P>CVS $Id: DistanceMatrix.java,v 1.5 2009/03/02 19:23:49 khuck Exp $</P>
  * @author khuck
  * @version 0.1
  * @since   0.1
  *
  */
public class DistanceMatrix {

    protected int dimension = 0;
    protected double[][] distances = null;
    
    /**
    * Default constructor - requires a dimension parameter.
    * 
    * @param dimension The height/width of the matrix.
    */
    public DistanceMatrix(int dimension) {
        this.dimension = dimension;
        this.distances = new double[dimension][dimension];
        for (int i = 0; i < dimension ; i++) {
            for (int j = i+1; j < dimension ; j++) {
                this.distances[i][j] = 1.0;
            }
        }
    }
    
    /**
    * Copy constructor - requires a DistanceMatrix parameter.
    * 
    * @param distances Another distance matrix to copy.
    */
    public DistanceMatrix(DistanceMatrix distances) {
        this.dimension = distances.dimension;
        this.distances = new double[dimension][dimension];
        for (int i = 0; i < dimension ; i++) {
            for (int j = 0; j < dimension ; j++) {
                this.distances[i][j] = distances.distances[i][j];
            }
        }
    }
    
    /**
    * Accessor method to get the dimension of the distance matrix.
    * 
    * @return
    */
    public int getDimension() {
        return this.dimension;
    }
    
    /**
    * Accessor method to get the value at a specified location.
    * 
    * @param x
    * @param y
    * @return
    */
    public double elementAt(int x, int y) {
        return distances[x][y];
    }
    
    /**
    * Create a sparse matrix of Manhattan distances.  This method populates
    * the matrix with the cartesian distances between the items.
    * 
    * @param data
    */
    public void solveManhattanDistances(RawDataInterface data) {
        for (int i = 0; i < dimension ; i++) {
            for (int j = 0; j < i ; j++) {
                distances[i][j] = data.getManhattanDistance(i,j);
            }
        }
    }
    
    /**
    * Create a sparse matrix of Cartesian distances.  This method populates 
    * the matrix with the cartesian distances between the items.
    * 
    * @param data
    */
    public void solveCartesianDistances(RawDataInterface data) {
        for (int i = 0; i < dimension ; i++) {
            for (int j = 0; j < i ; j++) {
                distances[i][j] = data.getCartesianDistance(i,j);
            }
        }
    }
    
    /**
     * Useful method for debugging.
     * 
     * @return
     */
    public String toString() {
        StringBuilder buf = new StringBuilder();
        for (int i = 0; i < dimension ; i++) {
            for (int j = 0; j < dimension ; j++) {
                buf.append(distances[i][j] + " ");
            }
            buf.append("\n");
        }		
        return buf.toString();
    }
    
    /**
    * Merge the distances between two vectors
    * 
    * I would prefer that this method use the Lance Williams dissimilarity
    * update formula algorithm, if I could find it...
    * 
    * @param x
    * @param y
    */
    public void mergeDistances (int x, int y) {
        for (int i = 0 ; i < x ; i++) {
            int z = dimension - (i+1);
            distances[x][i] = ((distances[x][i] * distances[x][z]) + 
                               (distances[y][i] * distances[y][z])) / 
                              (distances[x][z] + distances[y][z]);
            distances[x][z] = distances[x][z] + distances[y][z];
        }
        for (int i = y+1 ; i < dimension ; i++) {
            int z = dimension - (i+1);
            distances[i][x] = ((distances[i][x] * distances[z][x]) + 
                               (distances[i][y] * distances[z][y])) / 
                              (distances[z][x] + distances[z][y]);
            distances[z][x] = distances[z][x] + distances[z][y];
        }
    }
}
