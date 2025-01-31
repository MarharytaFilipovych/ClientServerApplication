import java.io.*;
import java.net.*;
import java.util.Arrays;

public class Client {
    static final int CHUNK = 1024;

    public static void main(String[] args) throws IOException {

        Socket socket = new Socket("localhost", 12345);

        PrintWriter out = new PrintWriter(socket.getOutputStream(), true);

        BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        out.println("Hello from client!");
        char[] buffer = new char[CHUNK];
        int bytesRead;
        if((bytesRead = in.read(buffer, 0, CHUNK)) != -1){
            String response = new String(buffer, 0, bytesRead);
            System.out.println("Server says: "+ response);
        }
        socket.close();

    }
}